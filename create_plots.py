#!/usr/bin/python3

# Read the output file produced by libmm.so.
# For every group of dirty pages (separated by empty line)
#   plot using plotter.plot script

import sys
import subprocess as sp

snap_i = 0

outf = open("tmp.dat"+str(snap_i), 'w')

with open(sys.argv[1], 'r') as inf:
    for line in inf:
        line = line.strip()
        if not line:
            # call gnuplot
            sp.run(["gnuplot", "-e", \
                        "outfile='snap_"+str(snap_i)+".png'; \
                        outtitle='"+str(snap_i)+"'; \
                        infile='tmp.dat"+str(snap_i)+"'",
                        "plotter.plot"
                    ])
            # re-open outf
            snap_i += 1
            outf.close()
            outf = open("tmp.dat"+str(snap_i), 'w')
        else:
            outf.write(line + "\n")
