# set outfile, outtitle, infile

set terminal pngcairo size 2048, 400
set output outfile

set yrange [0:1]

set title outtitle

plot infile using 1:2 with impulses lw 1
