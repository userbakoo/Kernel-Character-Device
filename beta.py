import struct
from time import time

import matplotlib.pyplot as plt
import numpy as np

if __name__ == '__main__':
    alphas = [2]
    betas = [2]
    counts_of_read_ints = [100001]

    for alpha in alphas:
        for beta in betas:
            for count_of_read_ints in counts_of_read_ints:
                with open("/dev/__prng_device", 'wb') as f:
                    f.write(f"beta: {alpha} {beta}".encode())

                size_of_int32 = 4
                np.random.seed(int(time()))
                data_rnd = np.random.beta(alpha, beta, int(count_of_read_ints))

                # print(data1)
                with open("/dev/__prng_device", 'rb', buffering=0) as f:
                    bytes_read = f.read(size_of_int32 * count_of_read_ints)
                    parsed = struct.unpack_from(f"<{count_of_read_ints}i", bytes_read)
                    #print(parsed)
                    plt.figure(figsize=(32, 12))

                    module = plt.subplot(221)
                    module.set_xlabel('Value', fontsize=15)
                    module.set_ylabel('Occurences', fontsize=15)
                    module.set_title(f'Module generator', fontsize=15)
                    normal = plt.subplot(222)
                    normal.set_xlabel('Value', fontsize=15)
                    normal.set_ylabel('Occurences', fontsize=15)
                    normal.set_title(f'Regular generator', fontsize=15)
                    #normal_hist, normal_bin_edges = np.histogram(data_rnd)
                    #normal_bin_edges = np.round(normal_bin_edges, 0)
                    #print("Normal hist:")
                    #print(normal_hist)
                    #print(normal_bin_edges)
                    #normal.bar(normal_bin_edges[:-1], normal_hist, width=0.5, color='gold', alpha=0.7)
                    normal.hist(data_rnd, bins=1000)
                    # normal.hist(data1, bins=1024, color="gold", density=True)
                    # module.hist(parsed, bins=range(min(parsed), max(parsed) + 2), color="gold")
                    #hist, bin_edges = np.histogram(parsed)
                    #bin_edges = np.round(bin_edges, 0)
                    #print("Module hist:")
                    #p#rint(hist)
                    #print(bin_edges)
                    #module.bar(bin_edges[:-1], hist, width=0.5, color='blue', alpha=0.7)
                    module.hist(parsed, bins=300)
                    plt.subplots_adjust(hspace=0.4)
                    plt.savefig(f'result_a{alpha}_b{beta}_count{count_of_read_ints}.png', dpi=300,
                                bbox_inches='tight')

                    # plt.show()
