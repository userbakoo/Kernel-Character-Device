import struct
import math

import matplotlib.colors
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from pandas.plotting._matplotlib.style import Color
from reportlab.lib.colors import black

if __name__ == '__main__':
    means = [0]
    std_devs = [1, 8, 32]
    counts_of_read_ints = [100000, 500000]

    for mean in means:
        for sd in std_devs:
            for count_of_read_ints in counts_of_read_ints:
                with open("/dev/__prng_device", 'wb') as f:
                    f.write(f"gauss: {sd} {mean}".encode())

                size_of_int32 = 4
                np.random.seed(42)
                data_rnd = np.random.normal(mean, sd, int(count_of_read_ints))
                data1 = [round(i) for i in data_rnd]
                #print(data1)
                with open("/dev/__prng_device", 'rb', buffering=0) as f:
                    bytes_read = f.read(size_of_int32 * count_of_read_ints)
                    parsed = struct.unpack_from(f"<{count_of_read_ints}i", bytes_read)

                    plt.figure(figsize=(32, 12))

                    both = plt.subplot(212)
                    both.hist(data1, bins=range(min(parsed), max(parsed) + 2), alpha=0.5, color="gold", density=True)
                    both.hist(parsed, bins=range(min(parsed), max(parsed) + 2), alpha=0.5, color="blue", density=True)
                    both.set_xlabel('Value', fontsize=15)
                    both.set_ylabel('Occurences', fontsize=15)
                    both.set_title(f'Comparison of module and regular generator mean={mean}, dev={sd}, n={count_of_read_ints}', fontsize=15)

                    module = plt.subplot(221)
                    module.set_xlabel('Value', fontsize=15)
                    module.set_ylabel('Occurences', fontsize=15)
                    module.set_title(f'Module generator mean={mean}, dev={sd}, n={count_of_read_ints}', fontsize=15)
                    normal = plt.subplot(222)
                    normal.set_xlabel('Value', fontsize=15)
                    normal.set_ylabel('Occurences', fontsize=15)
                    normal.set_title(f'Regular generator mean={mean}, dev={sd}, n={count_of_read_ints}', fontsize=15)
                    normal_hist, normal_bin_edges = np.histogram(data1, bins=range(min(parsed), max(parsed) + 2), density=True)
                    normal_bin_edges = np.round(normal_bin_edges, 0)
                    #print(normal_hist)
                    #print(normal_bin_edges)
                    normal.bar(normal_bin_edges[:-1], normal_hist, width=0.5, color='gold', alpha=0.7)
                    #normal.hist(data1, bins=1024, color="gold", density=True)
                    #module.hist(parsed, bins=range(min(parsed), max(parsed) + 2), color="gold")
                    hist, bin_edges = np.histogram(parsed, bins=range(min(parsed), max(parsed) + 2), density=True)
                    bin_edges = np.round(bin_edges, 0)
                    module.bar(bin_edges[:-1], hist, width=0.5, color='blue', alpha=0.7)
                    plt.subplots_adjust(hspace=0.4)
                    plt.savefig(f'Results/result_mean{mean}_dev{sd}_count{count_of_read_ints}.png', dpi=300, bbox_inches='tight')

                    #plt.show()

