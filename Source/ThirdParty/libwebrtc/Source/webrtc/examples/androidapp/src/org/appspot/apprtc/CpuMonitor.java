/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Scanner;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;

/**
 * Simple CPU monitor.  The caller creates a CpuMonitor object which can then
 * be used via sampleCpuUtilization() to collect the percentual use of the
 * cumulative CPU capacity for all CPUs running at their nominal frequency.  3
 * values are generated: (1) getCpuCurrent() returns the use since the last
 * sampleCpuUtilization(), (2) getCpuAvg3() returns the use since 3 prior
 * calls, and (3) getCpuAvgAll() returns the use over all SAMPLE_SAVE_NUMBER
 * calls.
 *
 * <p>CPUs in Android are often "offline", and while this of course means 0 Hz
 * as current frequency, in this state we cannot even get their nominal
 * frequency.  We therefore tread carefully, and allow any CPU to be missing.
 * Missing CPUs are assumed to have the same nominal frequency as any close
 * lower-numbered CPU, but as soon as it is online, we'll get their proper
 * frequency and remember it.  (Since CPU 0 in practice always seem to be
 * online, this unidirectional frequency inheritance should be no problem in
 * practice.)
 *
 * <p>Caveats:
 *   o No provision made for zany "turbo" mode, common in the x86 world.
 *   o No provision made for ARM big.LITTLE; if CPU n can switch behind our
 *     back, we might get incorrect estimates.
 *   o This is not thread-safe.  To call asynchronously, create different
 *     CpuMonitor objects.
 *
 * <p>If we can gather enough info to generate a sensible result,
 * sampleCpuUtilization returns true.  It is designed to never throw an
 * exception.
 *
 * <p>sampleCpuUtilization should not be called too often in its present form,
 * since then deltas would be small and the percent values would fluctuate and
 * be unreadable. If it is desirable to call it more often than say once per
 * second, one would need to increase SAMPLE_SAVE_NUMBER and probably use
 * Queue<Integer> to avoid copying overhead.
 *
 * <p>Known problems:
 *   1. Nexus 7 devices running Kitkat have a kernel which often output an
 *      incorrect 'idle' field in /proc/stat.  The value is close to twice the
 *      correct value, and then returns to back to correct reading.  Both when
 *      jumping up and back down we might create faulty CPU load readings.
 */
@TargetApi(Build.VERSION_CODES.KITKAT)
class CpuMonitor {
  private static final String TAG = "CpuMonitor";
  private static final int MOVING_AVERAGE_SAMPLES = 5;

  private static final int CPU_STAT_SAMPLE_PERIOD_MS = 2000;
  private static final int CPU_STAT_LOG_PERIOD_MS = 6000;

  private final Context appContext;
  // User CPU usage at current frequency.
  private final MovingAverage userCpuUsage;
  // System CPU usage at current frequency.
  private final MovingAverage systemCpuUsage;
  // Total CPU usage relative to maximum frequency.
  private final MovingAverage totalCpuUsage;
  // CPU frequency in percentage from maximum.
  private final MovingAverage frequencyScale;

  @Nullable
  private ScheduledExecutorService executor;
  private long lastStatLogTimeMs;
  private long[] cpuFreqMax;
  private int cpusPresent;
  private int actualCpusPresent;
  private boolean initialized;
  private boolean cpuOveruse;
  private String[] maxPath;
  private String[] curPath;
  private double[] curFreqScales;
  @Nullable
  private ProcStat lastProcStat;

  private static class ProcStat {
    final long userTime;
    final long systemTime;
    final long idleTime;

    ProcStat(long userTime, long systemTime, long idleTime) {
      this.userTime = userTime;
      this.systemTime = systemTime;
      this.idleTime = idleTime;
    }
  }

  private static class MovingAverage {
    private final int size;
    private double sum;
    private double currentValue;
    private double[] circBuffer;
    private int circBufferIndex;

    public MovingAverage(int size) {
      if (size <= 0) {
        throw new AssertionError("Size value in MovingAverage ctor should be positive.");
      }
      this.size = size;
      circBuffer = new double[size];
    }

    public void reset() {
      Arrays.fill(circBuffer, 0);
      circBufferIndex = 0;
      sum = 0;
      currentValue = 0;
    }

    public void addValue(double value) {
      sum -= circBuffer[circBufferIndex];
      circBuffer[circBufferIndex++] = value;
      currentValue = value;
      sum += value;
      if (circBufferIndex >= size) {
        circBufferIndex = 0;
      }
    }

    public double getCurrent() {
      return currentValue;
    }

    public double getAverage() {
      return sum / (double) size;
    }
  }

  public static boolean isSupported() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
        && Build.VERSION.SDK_INT < Build.VERSION_CODES.N;
  }

  public CpuMonitor(Context context) {
    if (!isSupported()) {
      throw new RuntimeException("CpuMonitor is not supported on this Android version.");
    }

    Log.d(TAG, "CpuMonitor ctor.");
    appContext = context.getApplicationContext();
    userCpuUsage = new MovingAverage(MOVING_AVERAGE_SAMPLES);
    systemCpuUsage = new MovingAverage(MOVING_AVERAGE_SAMPLES);
    totalCpuUsage = new MovingAverage(MOVING_AVERAGE_SAMPLES);
    frequencyScale = new MovingAverage(MOVING_AVERAGE_SAMPLES);
    lastStatLogTimeMs = SystemClock.elapsedRealtime();

    scheduleCpuUtilizationTask();
  }

  public void pause() {
    if (executor != null) {
      Log.d(TAG, "pause");
      executor.shutdownNow();
      executor = null;
    }
  }

  public void resume() {
    Log.d(TAG, "resume");
    resetStat();
    scheduleCpuUtilizationTask();
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void reset() {
    if (executor != null) {
      Log.d(TAG, "reset");
      resetStat();
      cpuOveruse = false;
    }
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized int getCpuUsageCurrent() {
    return doubleToPercent(userCpuUsage.getCurrent() + systemCpuUsage.getCurrent());
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized int getCpuUsageAverage() {
    return doubleToPercent(userCpuUsage.getAverage() + systemCpuUsage.getAverage());
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized int getFrequencyScaleAverage() {
    return doubleToPercent(frequencyScale.getAverage());
  }

  private void scheduleCpuUtilizationTask() {
    if (executor != null) {
      executor.shutdownNow();
      executor = null;
    }

    executor = Executors.newSingleThreadScheduledExecutor();
    @SuppressWarnings("unused") // Prevent downstream linter warnings.
    Future<?> possiblyIgnoredError = executor.scheduleAtFixedRate(new Runnable() {
      @Override
      public void run() {
        cpuUtilizationTask();
      }
    }, 0, CPU_STAT_SAMPLE_PERIOD_MS, TimeUnit.MILLISECONDS);
  }

  private void cpuUtilizationTask() {
    boolean cpuMonitorAvailable = sampleCpuUtilization();
    if (cpuMonitorAvailable
        && SystemClock.elapsedRealtime() - lastStatLogTimeMs >= CPU_STAT_LOG_PERIOD_MS) {
      lastStatLogTimeMs = SystemClock.elapsedRealtime();
      String statString = getStatString();
      Log.d(TAG, statString);
    }
  }

  private void init() {
    try (FileInputStream fin = new FileInputStream("/sys/devices/system/cpu/present");
         InputStreamReader streamReader = new InputStreamReader(fin, Charset.forName("UTF-8"));
         BufferedReader reader = new BufferedReader(streamReader);
         Scanner scanner = new Scanner(reader).useDelimiter("[-\n]");) {
      scanner.nextInt(); // Skip leading number 0.
      cpusPresent = 1 + scanner.nextInt();
      scanner.close();
    } catch (FileNotFoundException e) {
      Log.e(TAG, "Cannot do CPU stats since /sys/devices/system/cpu/present is missing");
    } catch (IOException e) {
      Log.e(TAG, "Error closing file");
    } catch (Exception e) {
      Log.e(TAG, "Cannot do CPU stats due to /sys/devices/system/cpu/present parsing problem");
    }

    cpuFreqMax = new long[cpusPresent];
    maxPath = new String[cpusPresent];
    curPath = new String[cpusPresent];
    curFreqScales = new double[cpusPresent];
    for (int i = 0; i < cpusPresent; i++) {
      cpuFreqMax[i] = 0; // Frequency "not yet determined".
      curFreqScales[i] = 0;
      maxPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/cpuinfo_max_freq";
      curPath[i] = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_cur_freq";
    }

    lastProcStat = new ProcStat(0, 0, 0);
    resetStat();

    initialized = true;
  }

  private synchronized void resetStat() {
    userCpuUsage.reset();
    systemCpuUsage.reset();
    totalCpuUsage.reset();
    frequencyScale.reset();
    lastStatLogTimeMs = SystemClock.elapsedRealtime();
  }

  private int getBatteryLevel() {
    // Use sticky broadcast with null receiver to read battery level once only.
    Intent intent = appContext.registerReceiver(
        null /* receiver */, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

    int batteryLevel = 0;
    int batteryScale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
    if (batteryScale > 0) {
      batteryLevel =
          (int) (100f * intent.getIntExtra(BatteryManager.EXTRA_LEVEL, 0) / batteryScale);
    }
    return batteryLevel;
  }

  /**
   * Re-measure CPU use.  Call this method at an interval of around 1/s.
   * This method returns true on success.  The fields
   * cpuCurrent, cpuAvg3, and cpuAvgAll are updated on success, and represents:
   * cpuCurrent: The CPU use since the last sampleCpuUtilization call.
   * cpuAvg3: The average CPU over the last 3 calls.
   * cpuAvgAll: The average CPU over the last SAMPLE_SAVE_NUMBER calls.
   */
  private synchronized boolean sampleCpuUtilization() {
    long lastSeenMaxFreq = 0;
    long cpuFreqCurSum = 0;
    long cpuFreqMaxSum = 0;

    if (!initialized) {
      init();
    }
    if (cpusPresent == 0) {
      return false;
    }

    actualCpusPresent = 0;
    for (int i = 0; i < cpusPresent; i++) {
      /*
       * For each CPU, attempt to first read its max frequency, then its
       * current frequency.  Once as the max frequency for a CPU is found,
       * save it in cpuFreqMax[].
       */

      curFreqScales[i] = 0;
      if (cpuFreqMax[i] == 0) {
        // We have never found this CPU's max frequency.  Attempt to read it.
        long cpufreqMax = readFreqFromFile(maxPath[i]);
        if (cpufreqMax > 0) {
          Log.d(TAG, "Core " + i + ". Max frequency: " + cpufreqMax);
          lastSeenMaxFreq = cpufreqMax;
          cpuFreqMax[i] = cpufreqMax;
          maxPath[i] = null; // Kill path to free its memory.
        }
      } else {
        lastSeenMaxFreq = cpuFreqMax[i]; // A valid, previously read value.
      }

      long cpuFreqCur = readFreqFromFile(curPath[i]);
      if (cpuFreqCur == 0 && lastSeenMaxFreq == 0) {
        // No current frequency information for this CPU core - ignore it.
        continue;
      }
      if (cpuFreqCur > 0) {
        actualCpusPresent++;
      }
      cpuFreqCurSum += cpuFreqCur;

      /* Here, lastSeenMaxFreq might come from
       * 1. cpuFreq[i], or
       * 2. a previous iteration, or
       * 3. a newly read value, or
       * 4. hypothetically from the pre-loop dummy.
       */
      cpuFreqMaxSum += lastSeenMaxFreq;
      if (lastSeenMaxFreq > 0) {
        curFreqScales[i] = (double) cpuFreqCur / lastSeenMaxFreq;
      }
    }

    if (cpuFreqCurSum == 0 || cpuFreqMaxSum == 0) {
      Log.e(TAG, "Could not read max or current frequency for any CPU");
      return false;
    }

    /*
     * Since the cycle counts are for the period between the last invocation
     * and this present one, we average the percentual CPU frequencies between
     * now and the beginning of the measurement period.  This is significantly
     * incorrect only if the frequencies have peeked or dropped in between the
     * invocations.
     */
    double currentFrequencyScale = cpuFreqCurSum / (double) cpuFreqMaxSum;
    if (frequencyScale.getCurrent() > 0) {
      currentFrequencyScale = (frequencyScale.getCurrent() + currentFrequencyScale) * 0.5;
    }

    ProcStat procStat = readProcStat();
    if (procStat == null) {
      return false;
    }

    long diffUserTime = procStat.userTime - lastProcStat.userTime;
    long diffSystemTime = procStat.systemTime - lastProcStat.systemTime;
    long diffIdleTime = procStat.idleTime - lastProcStat.idleTime;
    long allTime = diffUserTime + diffSystemTime + diffIdleTime;

    if (currentFrequencyScale == 0 || allTime == 0) {
      return false;
    }

    // Update statistics.
    frequencyScale.addValue(currentFrequencyScale);

    double currentUserCpuUsage = diffUserTime / (double) allTime;
    userCpuUsage.addValue(currentUserCpuUsage);

    double currentSystemCpuUsage = diffSystemTime / (double) allTime;
    systemCpuUsage.addValue(currentSystemCpuUsage);

    double currentTotalCpuUsage =
        (currentUserCpuUsage + currentSystemCpuUsage) * currentFrequencyScale;
    totalCpuUsage.addValue(currentTotalCpuUsage);

    // Save new measurements for next round's deltas.
    lastProcStat = procStat;

    return true;
  }

  private int doubleToPercent(double d) {
    return (int) (d * 100 + 0.5);
  }

  private synchronized String getStatString() {
    StringBuilder stat = new StringBuilder();
    stat.append("CPU User: ")
        .append(doubleToPercent(userCpuUsage.getCurrent()))
        .append("/")
        .append(doubleToPercent(userCpuUsage.getAverage()))
        .append(". System: ")
        .append(doubleToPercent(systemCpuUsage.getCurrent()))
        .append("/")
        .append(doubleToPercent(systemCpuUsage.getAverage()))
        .append(". Freq: ")
        .append(doubleToPercent(frequencyScale.getCurrent()))
        .append("/")
        .append(doubleToPercent(frequencyScale.getAverage()))
        .append(". Total usage: ")
        .append(doubleToPercent(totalCpuUsage.getCurrent()))
        .append("/")
        .append(doubleToPercent(totalCpuUsage.getAverage()))
        .append(". Cores: ")
        .append(actualCpusPresent);
    stat.append("( ");
    for (int i = 0; i < cpusPresent; i++) {
      stat.append(doubleToPercent(curFreqScales[i])).append(" ");
    }
    stat.append("). Battery: ").append(getBatteryLevel());
    if (cpuOveruse) {
      stat.append(". Overuse.");
    }
    return stat.toString();
  }

  /**
   * Read a single integer value from the named file.  Return the read value
   * or if an error occurs return 0.
   */
  private long readFreqFromFile(String fileName) {
    long number = 0;
    try (FileInputStream stream = new FileInputStream(fileName);
         InputStreamReader streamReader = new InputStreamReader(stream, Charset.forName("UTF-8"));
         BufferedReader reader = new BufferedReader(streamReader)) {
      String line = reader.readLine();
      number = parseLong(line);
    } catch (FileNotFoundException e) {
      // CPU core is off, so file with its scaling frequency .../cpufreq/scaling_cur_freq
      // is not present. This is not an error.
    } catch (IOException e) {
      // CPU core is off, so file with its scaling frequency .../cpufreq/scaling_cur_freq
      // is empty. This is not an error.
    }
    return number;
  }

  private static long parseLong(String value) {
    long number = 0;
    try {
      number = Long.parseLong(value);
    } catch (NumberFormatException e) {
      Log.e(TAG, "parseLong error.", e);
    }
    return number;
  }

  /*
   * Read the current utilization of all CPUs using the cumulative first line
   * of /proc/stat.
   */
  @SuppressWarnings("StringSplitter")
  private @Nullable ProcStat readProcStat() {
    long userTime = 0;
    long systemTime = 0;
    long idleTime = 0;
    try (FileInputStream stream = new FileInputStream("/proc/stat");
         InputStreamReader streamReader = new InputStreamReader(stream, Charset.forName("UTF-8"));
         BufferedReader reader = new BufferedReader(streamReader)) {
      // line should contain something like this:
      // cpu  5093818 271838 3512830 165934119 101374 447076 272086 0 0 0
      //       user    nice  system     idle   iowait  irq   softirq
      String line = reader.readLine();
      String[] lines = line.split("\\s+");
      int length = lines.length;
      if (length >= 5) {
        userTime = parseLong(lines[1]); // user
        userTime += parseLong(lines[2]); // nice
        systemTime = parseLong(lines[3]); // system
        idleTime = parseLong(lines[4]); // idle
      }
      if (length >= 8) {
        userTime += parseLong(lines[5]); // iowait
        systemTime += parseLong(lines[6]); // irq
        systemTime += parseLong(lines[7]); // softirq
      }
    } catch (FileNotFoundException e) {
      Log.e(TAG, "Cannot open /proc/stat for reading", e);
      return null;
    } catch (Exception e) {
      Log.e(TAG, "Problems parsing /proc/stat", e);
      return null;
    }
    return new ProcStat(userTime, systemTime, idleTime);
  }
}
