#!/usr/bin/env python3

import glob


def to_hex_list(data):
    out = []
    rows = [data[i : i + 16] for i in range(0, len(data), 16)]
    for i, x in enumerate(rows):
        row = ", ".join(["0x{val:02x}".format(val=c) for c in x])
        row += "," if i < len(rows) - 1 else ""
        out.append(f"  {row}")
    out.append("")
    return "\n".join(out)


for mod_file in glob.glob("mod/*.mod"):
    with open(mod_file, "rb") as f:
        data = f.read()
        out = to_hex_list(data)
        with open(mod_file + ".inc", "w") as f:
            f.write(out)
