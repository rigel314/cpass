#!/bin/bash

infile="$1"
outfile="$2"

sed -re 's/\\/\\\\/g' -e 's/^[ \t]+//' "$infile" | tr --delete '\n' | sed -re 's/"/\\"/g' -e 's/^/"/' -e 's/$/"/' > "$outfile"
