# Glyphic

Convert any TrueType font into interactive ASCII art with per‑character shaping, real‑time preview, and image export.

## Fetures

- Save rendered sentences as image via [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h)

## TODO

- [x] Add scroll bar with keybinds.
- [ ] Undo/redo
- [ ] Unicode / UTF-8 input support.
- [ ] Clipboard paste
- [ ] Pre-rasterize all 95 printable ASCII glyphs at the current em size into a lookup table so re-renders on keypress are instant instead of re-rasterizing every frame
- [ ] Add (fuzzy) search to search fonts.

---

Project is under [LICENSE](LICENSE).

---
