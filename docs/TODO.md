# T227U follow-up work

## CPU DVFS

- Add a persistent user-floor proc endpoint for the L/CPU 0-3 policy:
  `/proc/cpufreq/MT_CPU_DVFS_L/cpufreq_oc_min_freq`.
  Model its locking, PPM/thermal-envelope intersection, validation, and
  readback on the existing LL endpoint. Do not expose it in Kernel Adiutor
  until a direct device write/read/dwell test confirms that it survives PPM
  updates without weakening thermal or regulator constraints.
