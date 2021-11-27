import struct
import math

import matplotlib.colors
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

if __name__ == '__main__':
    with open("/dev/__prng_device", 'wb') as f:
        f.write("universal".encode())

    size_of_int32 = 4
    np.random.seed(42)
    count_of_read_ints = 100000
    high_value = 64
    data1 = np.random.uniform(0, high_value, count_of_read_ints)

    with open("/dev/__prng_device", 'rb', buffering=0) as f:
        bytes_read = f.read(size_of_int32 * count_of_read_ints)
        parsed = struct.unpack_from(f"<{count_of_read_ints}i", bytes_read)
        parsed_list = list(parsed)
        quotients = []
        for val in parsed_list:
            quotients.append(val % (high_value+1))

        module_hist, module_edges = np.histogram(quotients, bins=high_value+1, density=True)
        module_edges = np.round(module_edges, 0)

        python_hist, python_edges = np.histogram(data1, bins=high_value+1, density=True)
        python_edges = np.round(python_edges, 0)

        plt.figure(figsize=(32, 12))


        both = plt.subplot(212)
        both.bar(python_edges[:-1], python_hist, width=0.5, color='blue', alpha=0.5)
        both.bar(module_edges[:-1], module_hist, width=0.5, color='maroon', alpha=0.5)
        both.set_xlabel('Value', fontsize=15)
        both.set_ylabel('Probability', fontsize=15)
        both.set_title(f'Comparison of module and regular generator', fontsize=15)

        module = plt.subplot(221)
        module.set_xlabel('Value', fontsize=15)
        module.set_ylabel('Probability', fontsize=15)
        module.set_title(f'Module generator', fontsize=15)
        normal = plt.subplot(222)
        normal.set_xlabel('Value', fontsize=15)
        normal.set_ylabel('Probability', fontsize=15)
        normal.set_title(f'Regular generator', fontsize=15)

        normal.bar(python_edges[:-1], python_hist, width=0.5, color='blue', alpha=1)
        module.bar(module_edges[:-1], module_hist, width=0.5, color='maroon', alpha=1)

        plt.subplots_adjust(hspace=0.4)
        plt.savefig('Results/result_universal.png', dpi=300, bbox_inches='tight')
        #plt.show()