#!/usr/bin/env bash
# A script to run each interpreter binary multiple times,
# calculate average execution time, and produce chart in PDF and PNG formats.
# Dependencies: awk, gnuplot, time, date
# Copyright (c) 2015, 2016 Grigory Rechistov. All rights reserved.

# Set NITER to number of iterations to be done for each interpreter
NITER=5

# OPTS come from enviroment for sanity or debug runs
# OPTS=

### End of options ###
set -e
trap "exit" INT

export LANG=C
mkdir -p experiments
PFX=experiments/exp-`date +%s`
DATANAME=$PFX.txt
PLOTNAME=$PFX.plt
PDFNAME=$PFX.pdf
PNGNAME=$PFX.png

# Set VARIANTS to the list of all interpreters to compare
if [ -n $1 ]
then
    VARIANTS=$@
else
    VARIANTS="threaded-cached threaded switched predecoded subroutined"
fi

# Generate a comment for data file header 
echo "# Experiment at " `date` | tee -a $DATANAME
echo "# OS: " `uname -a` | tee -a $DATANAME
echo "# CPU " `cat /proc/cpuinfo |grep "model name" -m 1` | tee -a $DATANAME
echo "# Variants chosen: $VARIANTS, OPTS: $OPTS" | tee -a $DATANAME

for V in $VARIANTS
do
    echo -n "Measuring $V $NITER times"
    TIMENAME=$PFX-$V-time.txt
    for NTRY in `seq 1 $NITER`
    do
        /usr/bin/time -o $TIMENAME -a --format %e ./$V ${OPTS} > /dev/null
        echo -n .
    done
    echo " done"
    SUM=`awk '{s+=$1} END {print s}' $TIMENAME`
    AVG=`echo $SUM $NITER | awk '{print $1 / $2}'`
    # Generate data file entry
    echo $V $AVG | tee -a $DATANAME
done

# Generate Gnuplot script
echo "set terminal pdfcairo" >> $PLOTNAME
echo "set output \"$PDFNAME\"" >> $PLOTNAME
echo "set boxwidth 0.5" >> $PLOTNAME
echo "set key off" >> $PLOTNAME
echo "set xtics rotate by -45" >> $PLOTNAME
echo "set ylabel \"Average execution, seconds\"" >> $PLOTNAME
echo "set style fill solid" >> $PLOTNAME
echo "plot \"$DATANAME\" using 2:xtic(1) with boxes lc 3" >> $PLOTNAME
echo "set terminal pngcairo" >> $PLOTNAME
echo "set output \"$PNGNAME\"" >> $PLOTNAME
echo "replot" >> $PLOTNAME

# Run Gnuplot
gnuplot $PLOTNAME

echo "If all went well, results are in $PDFNAME and $PNGNAME"
