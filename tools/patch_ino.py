Import("env")
import os

def CustomFindInoNodes(env):
    src_dir = env.subst("$PROJECT_SRC_DIR")
    src_filter = env.get("SRC_FILTER")
    files = env.MatchSourceFiles(src_dir, src_filter, ["ino", "pde"])
    return [env.File(os.path.join(src_dir, f)) for f in files]

env.AddMethod(CustomFindInoNodes, "FindInoNodes")


