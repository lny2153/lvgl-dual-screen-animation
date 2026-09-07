"""Reproducible LVGL assets. Figma supplies static art; MiSans supplies editable glyphs.
No whole-screen movie, filesystem, browser or desktop API is used by GUI C code.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np, json, subprocess, io
ROOT=Path(__file__).resolve().parent
OUT=ROOT.parent
src=[]; declarations=[]
def image(name,im):
 alpha=im.getchannel('A').tobytes() if im.mode=='RGBA' else b''
 im=im.convert('RGB'); a=np.array(im,dtype=np.uint16)
 packed=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
 data=packed.astype('<u2').tobytes()+alpha
 cf='LV_COLOR_FORMAT_RGB565A8' if alpha else 'LV_COLOR_FORMAT_RGB565'
 src.append('static const uint8_t '+name+'_bytes[]={\n'+',\n'.join(','.join(str(b) for b in data[i:i+32]) for i in range(0,len(data),32))+'\n};\n')
 src.append(f'const lv_image_dsc_t {name}={{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf={cf},.w={im.width},.h={im.height},.stride={im.width*2}}},.data_size=sizeof({name}_bytes),.data={name}_bytes}};\n')
 declarations.append(f'LV_IMAGE_DECLARE({name});')
 return name
left=Image.open(ROOT/'left.png').convert('RGB');right=Image.open(ROOT/'right.png').convert('RGB')
data=left.crop((225,0,648,200))
fields=[(386,0,406,36,30),(364,120,416,194,84),(622,0,642,36,30),(596,120,648,194,84)]
fontpath=Path.home()/'AppData/Local/Microsoft/Windows/Fonts/MiSans-Light.ttf'
for i,(x,y,r,b,size) in enumerate(fields):
 w,h=r-x,b-y;zero=left.crop((x,y,r,b)); ink=zero.getbbox()
 font=ImageFont.truetype(str(fontpath),size*4)
 for c in '0123456789.-':
  if c=='0': glyph=zero
  else:
   fw=max(1,round(font.getlength(c)/4));glyph=Image.new('RGB',(fw*4,h*4))
   baseline=ink[3]*4-font.getbbox('0',anchor='ls')[3]
   ImageDraw.Draw(glyph).text((0,baseline),c,font=font,fill='white',anchor='ls')
   glyph=glyph.resize((fw,h),Image.Resampling.LANCZOS)
  image(f'gui_glyph_{i}_{ord(c)}',glyph)
 ImageDraw.Draw(data).rectangle((x-225,y,r-226,b-1),fill='black')
 src.append(f'const lv_image_dsc_t *const gui_digits_{i}[]={{'+','.join('&gui_glyph_'+str(i)+'_'+str(ord(c)) for c in '0123456789.-')+'};\n')
 declarations.append(f'extern const lv_image_dsc_t *const gui_digits_{i}[12];')
image('gui_text_art',data)
image('gui_eyes_art',left.crop((0,0,140,200)))
# Split right artwork into an independent ring and plus, without changing edge antialiasing.
ring=right.copy();ImageDraw.Draw(ring).rectangle((76,77,162,161),fill='black')
image('gui_ring_art',ring);image('gui_plus_art',right.crop((76,77,163,162)))
# Split the temporarily exported Figma food state into independently animated art and editable glyphs.
food=Image.open(ROOT/'left-food-reference.png').convert('RGB')
def keyed(im):
 a=np.array(im.convert('RGBA'));a[:,:,3]=np.where(np.any(a[:,:,:3]!=0,axis=2),255,0);return Image.fromarray(a)
corners=food.crop((0,0,140,140));ImageDraw.Draw(corners).rectangle((24,20,111,110),fill='black')
# Apple occupies the middle; corner shapes remain outside this crop.
art=Image.new('RGB',(140,140));art.paste(food.crop((24,20,112,111)),(24,20))
image('gui_corners_art',keyed(corners));image('gui_food_art',keyed(art))
image('gui_food_char_33529',food.crop((30,152,65,199)))
image('gui_food_char_26524',food.crop((65,152,101,199)))
(OUT/'GUI_Assets.c').write_text('#include "GUI_Assets.h"\n'+''.join(src),encoding='utf8')
(OUT/'GUI_Assets.h').write_text('#pragma once\n#include "lvgl.h"\n'+'\n'.join(declarations)+'\n',encoding='utf8')
print('Generated',len(src),'entries; C bytes', (OUT/'GUI_Assets.c').stat().st_size)
