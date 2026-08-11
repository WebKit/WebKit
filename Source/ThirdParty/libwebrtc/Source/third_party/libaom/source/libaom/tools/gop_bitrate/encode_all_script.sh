#!/bin/bash
#INPUT=media/cheer_sif.y4m
OUTPUT=test.webm
LIMIT=17
CPU_USED=3
CQ_LEVEL=36

for input in media/*
do
  echo "****" >> experiment.txt
  echo "input: $input" >> experiment.txt
  ./aomenc --limit=$LIMIT --codec=av1 --cpu-used=$CPU_USED --end-usage=q --cq-level=$CQ_LEVEL --psnr --threads=0 --profile=0 --lag-in-frames=35 --min-q=0 --max-q=63 --auto-alt-ref=1 --passes=2 --kf-max-dist=160 --kf-min-dist=0 --drop-frame=0 --static-thresh=0 --minsection-pct=0 --maxsection-pct=2000 --arnr-maxframes=7 --arnr-strength=5 --sharpness=0 --undershoot-pct=100 --overshoot-pct=100 --frame-parallel=0 --tile-columns=0 -o $OUTPUT $input >> experiment.txt
done
