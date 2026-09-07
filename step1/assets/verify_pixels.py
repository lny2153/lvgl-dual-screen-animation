from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np,json
p=Path(__file__).resolve().parent.parent;c=p/'captures';results={}
for f in c.glob('*.bmp'):Image.open(f).save(f.with_suffix('.png'))
for name,reference,capture in [('left','left.png','left-framework.png'),('right','right.png','right-framework.png'),('food','left-food-reference.png','left-food.png')]:
 a=np.array(Image.open(p/'assets'/reference).convert('RGB'),dtype=np.uint16)
 q=a.copy();q[:,:,0]=(a[:,:,0]>>3)*8+(a[:,:,0]>>5);q[:,:,1]=(a[:,:,1]>>2)*4+(a[:,:,1]>>6);q[:,:,2]=(a[:,:,2]>>3)*8+(a[:,:,2]>>5)
 b=np.array(Image.open(c/capture).convert('RGB'),dtype=np.uint16)
 mask=np.any(q!=b,axis=2);ys,xs=np.where(mask)
 results[name]={'different_pixels_rgb565':int(mask.sum()),'total_pixels':int(mask.size),'max_channel_delta':int(np.abs(q.astype(int)-b).max()),'difference_bbox':None if not len(xs) else [int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())]}
 Image.fromarray(q.astype('uint8')).save(c/f'{name}-reference-rgb565.png')
 diff=np.zeros_like(b,dtype='uint8');diff[mask]=[255,0,100];Image.fromarray(diff).save(c/f'{name}-difference.png')
(c/'pixel-report.json').write_text(json.dumps(results,indent=2),encoding='utf8');print(json.dumps(results))
assert all(r['different_pixels_rgb565']==0 for r in results.values()),results
