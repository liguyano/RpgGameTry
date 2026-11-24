Fountain split from spring.png

fountain_base.png: 362x362 fixed stone base, RGBA
fountain_water_12frames.png: 1448x1086, 4 cols x 3 rows, cell 362x362, RGBA
Render base and selected water frame at exactly the same top-left coordinate and identical scale.
Advance frames at ~9 FPS; keep the base static.
The water sheet removes stone; the base removes blue water. Overlay water after base.
