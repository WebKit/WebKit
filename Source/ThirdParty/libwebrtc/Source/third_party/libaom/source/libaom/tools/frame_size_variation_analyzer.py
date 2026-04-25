# RTC frame size variation analyzer
# Usage:
# 1. Config with "-DCONFIG_OUTPUT_FRAME_SIZE=1".
# 2. Build aomenc. Encode a file, and generate output file: frame_sizes.csv
# 3. Run: python ./frame_size.py frame_sizes.csv target-bitrate fps
#    Where target-bitrate: Bitrate (kbps), and fps is frame per second.
#    Example: python ../aom/tools/frame_size_variation_analyzer.py frame_sizes.csv
#    1000 30

import numpy as np
import csv
import sys
import matplotlib.pyplot as plt

# return the moving average
def moving_average(x, w):
  return np.convolve(x, np.ones(w), 'valid') / w

def frame_size_analysis(filename, target_br, fps):
  tbr = target_br * 1000 / fps

  with open(filename, 'r') as infile:
    raw_data = list(csv.reader(infile, delimiter=','))

  data = np.array(raw_data).astype(float)
  fsize = data[:, 0].astype(float)  # frame size
  qindex = data[:, 1].astype(float)  # qindex

  # Frame bit rate mismatch
  mismatch = np.absolute(fsize - np.full(fsize.size, tbr))

  # Count how many frames are more than 2.5x of frame target bit rate.
  tbr_thr = tbr * 2.5
  cnt = 0
  idx = np.arange(fsize.size)
  for i in idx:
    if fsize[i] > tbr_thr:
      cnt = cnt + 1

  # Use the 15-frame moving window
  win = 15
  avg_fsize = moving_average(fsize, win)
  win_mismatch = np.absolute(avg_fsize - np.full(avg_fsize.size, tbr))

  print('[Target frame rate (bit)]:', "%.2f"%tbr)
  print('[Average frame rate (bit)]:', "%.2f"%np.average(fsize))
  print('[Frame rate standard deviation]:', "%.2f"%np.std(fsize))
  print('[Max/min frame rate (bit)]:', "%.2f"%np.max(fsize), '/', "%.2f"%np.min(fsize))
  print('[Average frame rate mismatch (bit)]:', "%.2f"%np.average(mismatch))
  print('[Number of frames (frame rate > 2.5x of target frame rate)]:', cnt)
  print(' Moving window size:', win)
  print('[Moving average frame rate mismatch (bit)]:', "%.2f"%np.average(win_mismatch))
  print('------------------------------')

  figure, axis = plt.subplots(2)
  x = np.arange(fsize.size)
  axis[0].plot(x, fsize, color='blue')
  axis[0].set_title("frame sizes")
  axis[1].plot(x, qindex, color='blue')
  axis[1].set_title("frame qindex")
  plt.tight_layout()

  # Save the plot
  plotname = filename + '.png'
  plt.savefig(plotname)
  plt.show()

if __name__ == '__main__':
  if (len(sys.argv) < 4):
    print(sys.argv[0], 'input_file, target_bitrate, fps')
    sys.exit()
  target_br = int(sys.argv[2])
  fps = int(sys.argv[3])
  frame_size_analysis(sys.argv[1], target_br, fps)
