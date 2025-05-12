#!/usr/bin/env python3

import hashlib
from pathlib import Path

mods_dir = Path("../extras/songs/mod")

for mod_file in mods_dir.glob("*.wav"):
    with open(mod_file, "rb") as f:
        data = f.read()
        md5 = hashlib.md5(data)
        with open(Path("hash") / (Path(mod_file).name + ".md5"), "w") as f:
            f.write(md5.hexdigest())
