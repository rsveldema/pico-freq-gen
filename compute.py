


sys_clk = 150 * 1000 * 1000.0 # hz
samples_per_sec = 48000.0  # samples/sec
insns_per_sec = sys_clk
insns_per_sample = 1

wanted_freq = samples_per_sec

wave_secs = 1.0 / (2* wanted_freq)

insns_per_wave = wave_secs * insns_per_sec

print(f"insns/wave = {insns_per_wave}")