./make.sh
cat ar_bwb.jpg | jpegtopnm | ppmtopgm | ./imagetolaser --scale=0.03 >test.nc
