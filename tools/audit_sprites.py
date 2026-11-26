"""Audit actual sprite atlas pixel boundaries (8 rows x 10 columns)."""
from PIL import Image
from pathlib import Path

for file in ("hero.png", "torso_tunic.png"):
    im = Image.open(Path("assets") / file).convert("RGBA")
    w,h=im.size
    print(f"=== {file} {w}x{h} ===",flush=True)
    print("alpha_corners",*[im.getpixel(p)[3] for p in [(0,0),(w-1,0),(0,h-1),(w-1,h-1)]],flush=True)
    for axis in ("x","y"):
        totals=[]
        for i in range(w if axis=="x" else h):
            stride=3
            count=0
            for j in range(0,(h if axis=="x" else w),stride):
                r,g,b,a=im.getpixel((i,j) if axis=="x" else (j,i))
                # Opaque near-white checkerboard counts as background.
                if a>48 and min(r,g,b)<195 and max(r,g,b)<245:
                    count+=1
            totals.append(count)
        active=[k for k,v in enumerate(totals) if v>3]
        bands=[]
        for k in active:
            if not bands or k>bands[-1][-1]+1:bands.append([k])
            else:bands[-1].append(k)
        print(axis+"_bands",[(b[0],b[-1],max(totals[i] for i in b)) for b in bands if len(b)>2],flush=True)
    for row in range(8):
        row_info=[]
        for col in range(10):
            x0=round(w*col/10);x1=round(w*(col+1)/10)
            y0=round(h*row/8);y1=round(h*(row+1)/8)
            bbox=None;count=0
            for y in range(y0,y1,2):
                for x in range(x0,x1,2):
                    r,g,b,a=im.getpixel((x,y))
                    if a>48 and min(r,g,b)<195 and max(r,g,b)<245:
                        count+=1
                        bbox=[min(bbox[0],x),min(bbox[1],y),max(bbox[2],x),max(bbox[3],y)] if bbox else [x,y,x,y]
            row_info.append(f"{col}:{count}/{bbox}")
        print("row",row," ".join(row_info),flush=True)
