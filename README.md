# simplefuzzer

Simple fuzzer

simplefuzzer usage

    -b          use this byte for changes instead of arc4random (optional)

    -e          don't touch anything after this file offset (optional)

    -f          input file name

    -n          number of bytes to change in input file (default: 1)

    -o          fuzzed output file name (default: inputfile-fuzzed-date)

    -s          don't touch anything before this file offset (optional)

    -u          unique: don't change the same byte more than once (optional)
