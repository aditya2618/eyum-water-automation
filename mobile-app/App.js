import React, { useState, useEffect, useRef } from 'react';
import {
  StyleSheet,
  Text,
  View,
  TextInput,
  TouchableOpacity,
  ActivityIndicator,
  BackHandler,
  Platform,
  KeyboardAvoidingView,
  ScrollView,
  StatusBar,
} from 'react-native';
import { WebView } from 'react-native-webview';
import { Ionicons } from '@expo/vector-icons';

class ErrorBoundary extends React.Component {
  state = { hasError: false, error: null };
  static getDerivedStateFromError(error) {
    return { hasError: true, error };
  }
  componentDidCatch(error, info) {
    console.warn('Caught in ErrorBoundary:', error, info);
  }
  render() {
    if (this.state.hasError) {
      return (
        <View style={{ flex: 1, backgroundColor: '#090d16', justifyContent: 'center', alignItems: 'center', padding: 20 }}>
          <Ionicons name="alert-circle-outline" size={54} color="#ef4444" />
          <Text style={{ color: '#ffffff', fontSize: 18, fontWeight: '700', marginTop: 14 }}>Something went wrong</Text>
          <Text style={{ color: '#8094ae', fontSize: 13, textAlign: 'center', marginVertical: 12 }}>
            {this.state.error ? String(this.state.error.message || this.state.error) : 'Unknown error'}
          </Text>
          <TouchableOpacity
            style={{ backgroundColor: '#0072ff', paddingHorizontal: 22, paddingVertical: 12, borderRadius: 10, marginTop: 10 }}
            onPress={() => this.setState({ hasError: false, error: null })}
          >
            <Text style={{ color: '#ffffff', fontWeight: '700' }}>Retry</Text>
          </TouchableOpacity>
        </View>
      );
    }
    return this.props.children;
  }
}

const DEFAULT_URL = 'http://192.168.29.199';
const DEFAULT_USER = 'admin';
const DEFAULT_PASS = 'qwerty@1234';
const AP_URL = 'http://192.168.4.1';
const CONNECTION_TIMEOUT_MS = 8000;

// Dynamic status bar height for Android cutouts and notification area
const STATUS_BAR_SPACER =
  Platform.OS === 'android'
    ? Math.max((StatusBar.currentHeight || 0) + 16, 48)
    : 44;

// Base64 helper for HTTP Basic Authentication
const toBase64 = (str) => {
  try {
    return btoa(str);
  } catch (e) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
    let output = '';
    for (
      let block = 0, charCode, idx = 0, map = chars;
      str.charAt(idx | 0) || ((map = '='), idx % 1);
      output += map.charAt(63 & (block >> (8 - (idx % 1) * 8)))
    ) {
      charCode = str.charCodeAt((idx += 3 / 4));
      block = (block << 8) | charCode;
    }
    return output;
  }
};

export default function App() {
  const webViewRef = useRef(null);
  const timeoutRef = useRef(null);

  // Screen navigation state: 'login' | 'dashboard'
  const [currentScreen, setCurrentScreen] = useState('login');

  // Connection and Credentials
  const [controllerUrl, setControllerUrl] = useState(DEFAULT_URL);
  const [username, setUsername] = useState(DEFAULT_USER);
  const [password, setPassword] = useState(DEFAULT_PASS);
  const [showPassword, setShowPassword] = useState(false);

  // WebView Loading & Error states
  const [isLoading, setIsLoading] = useState(true);
  const [hasError, setHasError] = useState(false);
  const [errorDetails, setErrorDetails] = useState('');
  const [canGoBack, setCanGoBack] = useState(false);
  const [keyCounter, setKeyCounter] = useState(0);

  const authHeader = 'Basic ' + toBase64(`${username}:${password}`);

  // Injected JavaScript so all background in-dashboard fetch('/api/...') calls inherit HTTP Basic Auth
  const injectedAuthScript = `
    (function() {
      const headerVal = '${authHeader}';
      const origFetch = window.fetch;
      window.fetch = function(url, options) {
        options = options || {};
        options.headers = options.headers || {};
        if (options.headers instanceof Headers) {
          if (!options.headers.has('Authorization')) {
            options.headers.append('Authorization', headerVal);
          }
        } else if (typeof options.headers === 'object') {
          if (!options.headers['Authorization']) {
            options.headers['Authorization'] = headerVal;
          }
        }
        return origFetch(url, options);
      };
    })();
    true;
  `;

  const clearWatchdog = () => {
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
      timeoutRef.current = null;
    }
  };

  const startWatchdog = (url) => {
    clearWatchdog();
    timeoutRef.current = setTimeout(() => {
      setIsLoading(false);
      setHasError(true);
      if (url.includes('.local')) {
        setErrorDetails(
          'Connection timed out. Android devices cannot resolve ".local" domains. Please use the ESP32 IP address (e.g. 192.168.29.199).'
        );
      } else {
        setErrorDetails(
          `Connection timed out reaching ${url}. Please verify your phone is connected to the same Wi-Fi as the ESP32.`
        );
      }
    }, CONNECTION_TIMEOUT_MS);
  };

  // Android hardware back button handler
  useEffect(() => {
    if (Platform.OS !== 'android') return;

    const onBackPress = () => {
      if (currentScreen === 'dashboard') {
        if (canGoBack && webViewRef.current) {
          webViewRef.current.goBack();
          return true;
        }
        // If cannot go back further in WebView, return to Login screen
        setCurrentScreen('login');
        return true;
      }
      return false;
    };

    let subscription;
    try {
      if (BackHandler && typeof BackHandler.addEventListener === 'function') {
        subscription = BackHandler.addEventListener('hardwareBackPress', onBackPress);
      }
    } catch (e) {
      console.warn('BackHandler registration skipped:', e);
    }

    return () => {
      try {
        if (subscription && typeof subscription.remove === 'function') {
          subscription.remove();
        } else if (BackHandler && typeof BackHandler.removeEventListener === 'function') {
          BackHandler.removeEventListener('hardwareBackPress', onBackPress);
        }
      } catch (e) {}
    };
  }, [currentScreen, canGoBack]);

  const formatUrl = (input) => {
    let clean = input.trim();
    if (!clean.startsWith('http://') && !clean.startsWith('https://')) {
      clean = 'http://' + clean;
    }
    return clean.replace(/\/+$/, '');
  };

  // Handle Login submission
  const handleConnect = () => {
    const formatted = formatUrl(controllerUrl);
    setControllerUrl(formatted);
    setHasError(false);
    setIsLoading(true);
    setKeyCounter((prev) => prev + 1);
    setCurrentScreen('dashboard');
    startWatchdog(formatted);
    setTimeout(() => {
      setIsLoading(false);
    }, 2000);
  };

  const handleReload = () => {
    setHasError(false);
    setIsLoading(true);
    startWatchdog(controllerUrl);
    setTimeout(() => {
      setIsLoading(false);
    }, 2000);
    if (webViewRef.current) {
      webViewRef.current.reload();
    } else {
      setKeyCounter((prev) => prev + 1);
    }
  };

  const handleReturnToLogin = () => {
    clearWatchdog();
    setCurrentScreen('login');
  };

  return (
    <ErrorBoundary>
      <View style={styles.safeArea}>
        <StatusBar barStyle="light-content" backgroundColor="#090d16" translucent={true} />
        {/* Dedicated Top Spacer for Android Status Bar / Notification Icons / Camera Notch */}
        <View style={{ height: STATUS_BAR_SPACER, backgroundColor: '#090d16', width: '100%' }} />

        {currentScreen === 'login' ? (
          /* ================================================================= */
          /*                       LOGIN / AUTHENTICATION PAGE                 */
          /* ================================================================= */
          <KeyboardAvoidingView
            behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
            style={styles.keyboardContainer}
          >
            <ScrollView
              contentContainerStyle={styles.loginScrollContainer}
              keyboardShouldPersistTaps="handled"
            >
              {/* Brand Header */}
              <View style={styles.loginHeader}>
                <View style={styles.logoCircle}>
                  <Ionicons name="water" size={44} color="#00c6ff" />
                </View>
                <Text style={styles.appTitle}>iYum Water Automation</Text>
                <Text style={styles.appSubtitle}>Controller Login & Authentication</Text>
              </View>

              {/* Login Card */}
              <View style={styles.loginCard}>
                <Text style={styles.cardSectionTitle}>CONNECT TO CONTROLLER</Text>

                {/* Controller URL Input */}
                <View style={styles.inputGroup}>
                  <Text style={styles.inputLabel}>ESP32 Controller Address / IP</Text>
                  <View style={styles.inputWrapper}>
                    <Ionicons name="wifi-outline" size={18} color="#58a6ff" style={styles.inputIcon} />
                    <TextInput
                      style={styles.textInput}
                      value={controllerUrl}
                      onChangeText={setControllerUrl}
                      placeholder="e.g. 192.168.29.199 or 192.168.4.1"
                      placeholderTextColor="#484f58"
                      autoCapitalize="none"
                      autoCorrect={false}
                      keyboardType="url"
                    />
                  </View>
                </View>

                {/* Preset Chips */}
                <View style={styles.presetRow}>
                  <TouchableOpacity
                    style={[
                      styles.presetChip,
                      controllerUrl.includes('192.168.29.199') && styles.presetChipActive,
                    ]}
                    onPress={() => setControllerUrl(DEFAULT_URL)}
                  >
                    <Ionicons name="hardware-chip-outline" size={13} color="#00c6ff" style={{ marginRight: 4 }} />
                    <Text style={styles.presetChipText}>192.168.29.199</Text>
                  </TouchableOpacity>

                  <TouchableOpacity
                    style={[
                      styles.presetChip,
                      controllerUrl.includes('192.168.4.1') && styles.presetChipActive,
                    ]}
                    onPress={() => setControllerUrl(AP_URL)}
                  >
                    <Ionicons name="radio-outline" size={13} color="#10b981" style={{ marginRight: 4 }} />
                    <Text style={styles.presetChipText}>192.168.4.1 (AP)</Text>
                  </TouchableOpacity>

                  <TouchableOpacity
                    style={[
                      styles.presetChip,
                      controllerUrl.includes('iyum-water.local') && styles.presetChipActive,
                    ]}
                    onPress={() => setControllerUrl('http://iyum-water.local')}
                  >
                    <Ionicons name="globe-outline" size={13} color="#8094ae" style={{ marginRight: 4 }} />
                    <Text style={styles.presetChipText}>iyum-water.local</Text>
                  </TouchableOpacity>
                </View>

                {/* Username Input */}
                <View style={styles.inputGroup}>
                  <Text style={styles.inputLabel}>Username</Text>
                  <View style={styles.inputWrapper}>
                    <Ionicons name="person-outline" size={18} color="#58a6ff" style={styles.inputIcon} />
                    <TextInput
                      style={styles.textInput}
                      value={username}
                      onChangeText={setUsername}
                      placeholder="admin"
                      placeholderTextColor="#484f58"
                      autoCapitalize="none"
                      autoCorrect={false}
                    />
                  </View>
                </View>

                {/* Password Input */}
                <View style={styles.inputGroup}>
                  <Text style={styles.inputLabel}>Admin Password</Text>
                  <View style={styles.inputWrapper}>
                    <Ionicons name="lock-closed-outline" size={18} color="#58a6ff" style={styles.inputIcon} />
                    <TextInput
                      style={[styles.textInput, { flex: 1 }]}
                      value={password}
                      onChangeText={setPassword}
                      placeholder="e.g. qwerty@1234 or 12345678"
                      placeholderTextColor="#484f58"
                      secureTextEntry={!showPassword}
                      autoCapitalize="none"
                      autoCorrect={false}
                    />
                    <TouchableOpacity
                      onPress={() => setShowPassword(!showPassword)}
                      style={styles.eyeBtn}
                      activeOpacity={0.7}
                    >
                      <Ionicons
                        name={showPassword ? 'eye-off-outline' : 'eye-outline'}
                        size={20}
                        color="#8094ae"
                      />
                    </TouchableOpacity>
                  </View>
                </View>

                {/* Connect Button */}
                <TouchableOpacity
                  style={styles.connectButton}
                  onPress={handleConnect}
                  activeOpacity={0.8}
                >
                  <Ionicons name="log-in-outline" size={20} color="#ffffff" style={{ marginRight: 8 }} />
                  <Text style={styles.connectButtonText}>Enter Water Dashboard</Text>
                </TouchableOpacity>

                {/* Quick Hint */}
                <View style={styles.hintContainer}>
                  <Ionicons name="information-circle-outline" size={15} color="#00c6ff" style={{ marginRight: 6 }} />
                  <Text style={styles.hintText}>
                    Default credentials: <Text style={styles.hintHighlight}>admin</Text> / <Text style={styles.hintHighlight}>qwerty@1234</Text>
                  </Text>
                </View>
              </View>

              <Text style={styles.footerText}>iYum Water Automation • Pure Local IoT</Text>
            </ScrollView>
          </KeyboardAvoidingView>
        ) : (
          /* ================================================================= */
          /*                       DASHBOARD VIEW SCREEN                       */
          /* ================================================================= */
          <View style={styles.dashboardContainer}>
            {/* Content Area - Clean Full-Screen Dashboard */}
            <View style={styles.webviewContainer}>
              {!hasError ? (
                <View style={styles.webviewWrapper}>
                  <WebView
                    key={`webview-${keyCounter}-${controllerUrl}-${username}-${password}`}
                    ref={webViewRef}
                    source={{
                      uri: controllerUrl,
                      headers: {
                        Authorization: authHeader,
                      },
                    }}
                    basicAuthCredential={{
                      username: username,
                      password: password,
                    }}
                    injectedJavaScriptBeforeContentLoaded={injectedAuthScript}
                    style={styles.webview}
                    javaScriptEnabled={true}
                    domStorageEnabled={true}
                    allowsInlineMediaPlayback={true}
                    allowFileAccess={false}
                    allowFileAccessFromFileURLs={false}
                    originWhitelist={['*']}
                    mixedContentMode="always"
                    pullToRefreshEnabled={true}
                    onLoadStart={() => {
                      setIsLoading(true);
                      setHasError(false);
                    }}
                    onLoadEnd={() => {
                      clearWatchdog();
                      setIsLoading(false);
                    }}
                    onError={(syntheticEvent) => {
                      clearWatchdog();
                      const { nativeEvent } = syntheticEvent;
                      setHasError(true);
                      setIsLoading(false);
                      setErrorDetails(
                        controllerUrl.includes('.local')
                          ? 'Android cannot resolve ".local" domains. Please enter the ESP32 IP address (e.g. 192.168.29.199).'
                          : nativeEvent.description || 'Connection failed or host unreachable'
                      );
                    }}
                    onHttpError={(syntheticEvent) => {
                      const { nativeEvent } = syntheticEvent;
                      if (nativeEvent.statusCode === 401) {
                        clearWatchdog();
                        setHasError(true);
                        setIsLoading(false);
                        setErrorDetails('Authentication Failed: Invalid username or password (HTTP 401).');
                      } else if (nativeEvent.statusCode >= 400) {
                        clearWatchdog();
                        setHasError(true);
                        setIsLoading(false);
                        setErrorDetails(`HTTP Error ${nativeEvent.statusCode}`);
                      }
                    }}
                    onNavigationStateChange={(navState) => {
                      setCanGoBack(navState.canGoBack);
                      if (!navState.loading) {
                        clearWatchdog();
                        setIsLoading(false);
                      }
                    }}
                  />

                  {/* Slim Top Progress Indicator */}
                  {isLoading && (
                    <View style={styles.topProgressBanner}>
                      <ActivityIndicator size="small" color="#00c6ff" style={{ marginRight: 8 }} />
                      <Text style={styles.topProgressText}>Connecting to {controllerUrl}...</Text>
                    </View>
                  )}

                  {/* Floating Action Button for Auth / IP Settings */}
                  <TouchableOpacity
                    style={styles.floatingAuthBtn}
                    onPress={handleReturnToLogin}
                    accessibilityLabel="Controller IP and Auth Settings"
                    activeOpacity={0.8}
                  >
                    <Ionicons name="key-outline" size={20} color="#00c6ff" />
                  </TouchableOpacity>
                </View>
              ) : (
                /* Error Screen */
                <ScrollView contentContainerStyle={styles.errorContainer}>
                  <View style={styles.errorCard}>
                    <View style={styles.errorIconCircle}>
                      <Ionicons name="alert-circle-outline" size={48} color="#ef4444" />
                    </View>

                    <Text style={styles.errorTitle}>Connection Failed</Text>
                    <Text style={styles.errorTarget}>{controllerUrl}</Text>

                    {errorDetails ? (
                      <View style={styles.errorBox}>
                        <Text style={styles.errorMsgText}>{errorDetails}</Text>
                      </View>
                    ) : null}

                    <View style={styles.troubleshootCard}>
                      <Text style={styles.troubleshootTitle}>Check the following:</Text>
                      <Text style={styles.troubleshootLine}>
                        1. Are your phone and ESP32 on the same Wi-Fi network?
                      </Text>
                      <Text style={styles.troubleshootLine}>
                        2. Is the password correct? Default is <Text style={styles.codeText}>qwerty@1234</Text>.
                      </Text>
                      <Text style={styles.troubleshootLine}>
                        3. If in AP setup mode, connect to <Text style={styles.codeText}>iYum-Water-Setup</Text> and use <Text style={styles.codeText}>192.168.4.1</Text>.
                      </Text>
                    </View>

                    <View style={styles.actionRow}>
                      <TouchableOpacity
                        style={[styles.btn, styles.btnPrimary]}
                        onPress={handleReturnToLogin}
                        activeOpacity={0.8}
                      >
                        <Ionicons name="key-outline" size={18} color="#ffffff" style={{ marginRight: 6 }} />
                        <Text style={styles.btnTextPrimary}>Change Password / IP</Text>
                      </TouchableOpacity>

                      <TouchableOpacity
                        style={[styles.btn, styles.btnSecondary]}
                        onPress={handleReload}
                        activeOpacity={0.8}
                      >
                        <Ionicons name="refresh" size={18} color="#00c6ff" style={{ marginRight: 6 }} />
                        <Text style={styles.btnTextSecondary}>Retry</Text>
                      </TouchableOpacity>
                    </View>
                  </View>
                </ScrollView>
              )}
            </View>
          </View>
        )}
        </View>
      </ErrorBoundary>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: '#090d16',
    paddingTop: 0,
  },
  keyboardContainer: {
    flex: 1,
  },
  loginScrollContainer: {
    flexGrow: 1,
    justifyContent: 'center',
    padding: 20,
    backgroundColor: '#090d16',
  },
  loginHeader: {
    alignItems: 'center',
    marginBottom: 24,
  },
  logoCircle: {
    width: 84,
    height: 84,
    borderRadius: 42,
    backgroundColor: '#131c2e',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 14,
    borderWidth: 1.5,
    borderColor: 'rgba(0, 198, 255, 0.4)',
    shadowColor: '#00c6ff',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.5,
    shadowRadius: 12,
    elevation: 8,
  },
  logoImage: {
    width: 58,
    height: 58,
    borderRadius: 12,
  },
  appTitle: {
    fontSize: 22,
    fontWeight: '800',
    color: '#ffffff',
    letterSpacing: 0.5,
  },
  appSubtitle: {
    fontSize: 13,
    color: '#8094ae',
    marginTop: 4,
    fontWeight: '500',
  },
  loginCard: {
    backgroundColor: '#131c2e',
    borderRadius: 18,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.08)',
    padding: 22,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 10,
    elevation: 6,
  },
  cardSectionTitle: {
    fontSize: 11,
    fontWeight: '700',
    color: '#00c6ff',
    letterSpacing: 1.2,
    marginBottom: 16,
  },
  inputGroup: {
    marginBottom: 16,
  },
  inputLabel: {
    fontSize: 12,
    fontWeight: '600',
    color: '#c9d1d9',
    marginBottom: 6,
  },
  inputWrapper: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#090d16',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.12)',
    borderRadius: 10,
    paddingHorizontal: 12,
    height: 48,
  },
  inputIcon: {
    marginRight: 10,
  },
  textInput: {
    flex: 1,
    color: '#ffffff',
    fontSize: 15,
  },
  eyeBtn: {
    padding: 6,
  },
  presetRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: -6,
    marginBottom: 16,
  },
  presetChip: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1a233a',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.1)',
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 16,
  },
  presetChipActive: {
    borderColor: '#00c6ff',
    backgroundColor: 'rgba(0, 198, 255, 0.12)',
  },
  presetChipText: {
    color: '#c9d1d9',
    fontSize: 11,
    fontWeight: '600',
  },
  connectButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#0072ff',
    height: 50,
    borderRadius: 12,
    marginTop: 8,
    shadowColor: '#0072ff',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.4,
    shadowRadius: 6,
    elevation: 4,
  },
  connectButtonText: {
    color: '#ffffff',
    fontSize: 16,
    fontWeight: '700',
    letterSpacing: 0.4,
  },
  hintContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    marginTop: 16,
    backgroundColor: 'rgba(0, 198, 255, 0.06)',
    paddingVertical: 8,
    paddingHorizontal: 12,
    borderRadius: 8,
  },
  hintText: {
    fontSize: 12,
    color: '#8094ae',
  },
  hintHighlight: {
    color: '#00c6ff',
    fontWeight: '600',
  },
  footerText: {
    textAlign: 'center',
    color: '#4b5563',
    fontSize: 11,
    marginTop: 22,
  },

  /* Dashboard View Styles */
  dashboardContainer: {
    flex: 1,
    backgroundColor: '#090d16',
  },
  floatingAuthBtn: {
    position: 'absolute',
    bottom: 24,
    right: 20,
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: '#131c2e',
    alignItems: 'center',
    justifyContent: 'center',
    borderWidth: 1.5,
    borderColor: 'rgba(0, 198, 255, 0.4)',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.5,
    shadowRadius: 6,
    elevation: 8,
    zIndex: 999,
  },
  statusDot: {
    width: 9,
    height: 9,
    borderRadius: 4.5,
    marginRight: 9,
  },
  statusDotOk: {
    backgroundColor: '#10b981',
    shadowColor: '#10b981',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 4,
  },
  statusDotLoading: {
    backgroundColor: '#f59e0b',
  },
  statusDotError: {
    backgroundColor: '#ef4444',
    shadowColor: '#ef4444',
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 4,
  },
  headerTitle: {
    fontSize: 15,
    fontWeight: '700',
    color: '#ffffff',
  },
  headerSubtitle: {
    fontSize: 11,
    color: '#8094ae',
  },
  headerRight: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  navBtn: {
    padding: 8,
    marginLeft: 6,
    borderRadius: 8,
    backgroundColor: '#1a233a',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.08)',
  },
  navBtnLogin: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 10,
    backgroundColor: 'rgba(0, 198, 255, 0.1)',
    borderColor: 'rgba(0, 198, 255, 0.3)',
  },
  navBtnLoginText: {
    color: '#00c6ff',
    fontSize: 12,
    fontWeight: '600',
  },
  webviewContainer: {
    flex: 1,
    backgroundColor: '#090d16',
  },
  webviewWrapper: {
    flex: 1,
    position: 'relative',
  },
  webview: {
    flex: 1,
    backgroundColor: '#090d16',
  },
  topProgressBanner: {
    height: 34,
    backgroundColor: '#131c2e',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 198, 255, 0.3)',
  },
  topProgressText: {
    color: '#00c6ff',
    fontSize: 12,
    fontWeight: '600',
  },
  errorContainer: {
    flexGrow: 1,
    justifyContent: 'center',
    padding: 20,
    backgroundColor: '#090d16',
  },
  errorCard: {
    backgroundColor: '#131c2e',
    borderRadius: 16,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.08)',
    padding: 22,
    alignItems: 'center',
  },
  errorIconCircle: {
    width: 72,
    height: 72,
    borderRadius: 36,
    backgroundColor: 'rgba(239, 68, 68, 0.1)',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 14,
    borderWidth: 1,
    borderColor: 'rgba(239, 68, 68, 0.25)',
  },
  errorTitle: {
    fontSize: 18,
    fontWeight: '700',
    color: '#ffffff',
    textAlign: 'center',
  },
  errorTarget: {
    fontSize: 13,
    color: '#00c6ff',
    marginTop: 4,
    marginBottom: 12,
    fontWeight: '500',
  },
  errorBox: {
    backgroundColor: 'rgba(239, 68, 68, 0.08)',
    borderWidth: 1,
    borderColor: 'rgba(239, 68, 68, 0.25)',
    padding: 10,
    borderRadius: 8,
    marginBottom: 16,
    width: '100%',
  },
  errorMsgText: {
    fontSize: 12,
    color: '#f87171',
    textAlign: 'center',
    lineHeight: 17,
  },
  troubleshootCard: {
    width: '100%',
    backgroundColor: '#090d16',
    borderRadius: 10,
    padding: 14,
    marginBottom: 18,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.06)',
  },
  troubleshootTitle: {
    fontSize: 13,
    fontWeight: '600',
    color: '#e2e8f0',
    marginBottom: 8,
  },
  troubleshootLine: {
    fontSize: 12,
    color: '#8094ae',
    lineHeight: 18,
    marginBottom: 6,
  },
  codeText: {
    color: '#00c6ff',
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
    fontWeight: '600',
  },
  actionRow: {
    flexDirection: 'row',
    width: '100%',
    justifyContent: 'space-between',
    gap: 10,
  },
  btn: {
    flex: 1,
    height: 46,
    borderRadius: 10,
    flexDirection: 'row',
    justifyContent: 'center',
    alignItems: 'center',
  },
  btnPrimary: {
    backgroundColor: '#0072ff',
  },
  btnTextPrimary: {
    color: '#ffffff',
    fontSize: 13,
    fontWeight: '600',
  },
  btnSecondary: {
    backgroundColor: '#1a233a',
    borderWidth: 1,
    borderColor: 'rgba(0, 198, 255, 0.3)',
  },
  btnTextSecondary: {
    color: '#00c6ff',
    fontSize: 13,
    fontWeight: '600',
  },
});
