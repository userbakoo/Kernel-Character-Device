# Kernel-Character-Device

### Building
    mkdir build
    cd build
    cmake ..
    make module

<p>Results in <strong>my_module.ko</strong> file.</p>
<p>Header paths might not line up on all distributions & versions.</p>
<p>Inspect & edit FindKernelHeaders.cmake to line them up. Most common culprit would be the find_path on line 7.</p>

### Launching
    cd build
    insmod my_module.ko

<p>This generates a character device in /dev/ under the name "__prng_device".</p>
<p>Remove the module with:</p>

    rmmod my_module
### Usage
<p> Character device can be opened and read eg:</p>

    cat /dev/__prng_device | head -c 128 | hexdump -C
<p> Or: </p>

    dd if=/dev/__prng_device bs=128 count=1 2>/dev/null | hexdump -C

<p> Character device can be opened and written to overwrite the seed: </p>

    echo "aaaabbbbccccdddd" > /dev/__prng_device