# Songs

### Content

Contents of the current directory:
+ [List](songs.lst) of links pointing to tiny (lesser than 20 kiB) MOD files from [The Mod Archive](https://modarchive.org).
+ [Shell script](download.sh) for downloading MODs. Works also in Git Bash from Windows.
+ [Python script](mod-to-inc.py) to convert downloaded MODs to `*inc` files.

---

### Inc Files

These files can be inserted into C++ code source in such manner (see [HelloMod.ino](../../examples/HelloMod/HelloMod.ino)):
```cpp
static const uint8_t PROGMEM g_song[] = {
#include "between2.mod.inc"
};
```

To generate them:
+ Place the MOD files in the [mod](mod) folder (or use [download.sh](download.sh) to take files from `The Mod Archive`) 
+ Run the command: `python mod-to-inc.py`.

---

### Licensing and Copyright

The copyright of files downloaded from `The Mod Archive` belongs to their creators. For more information, please read [Licensing Modules and Copyright](https://modarchive.org/index.php?faq-licensing) article.
This repository only contains links to these files, which are used for library testing purposes only.
