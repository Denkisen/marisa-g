import os
import sys
import math
import PIL
from PIL import Image

if len (sys.argv) == 1 or not os.path.exists(sys.argv[1]) or not os.path.isfile(sys.argv[1]):
  sys.exit(-1)

base_filename = sys.argv[1]
result_filepath, result_file_name = os.path.split(base_filename)
result_file_name = ".".join(result_file_name.split('.')[:-1]) + "_mip." + result_file_name.split('.')[-1]
result_filepath = os.path.join(result_filepath, result_file_name)
image = Image.open(sys.argv[1])
width, height  = image.size
mip_levels = int(math.floor(math.log(max([width, height]), 2))) + 1
res_image = Image.new(image.mode, (width + (width // 2), height))
res_image.paste(image, (0,0))

tmp_h = 0
tmp_width = width
tmp_height = height
for i in range(1, mip_levels):
  tmp_height = tmp_height // 2
  tmp_width = tmp_width // 2
  small_img = image.resize(resample=PIL.Image.BICUBIC, size=(tmp_width, tmp_height))
  res_image.paste(small_img, (width, tmp_h))
  tmp_h += tmp_height

print(height + tmp_h)
res_image.save(result_filepath)