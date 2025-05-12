#!/usr/bin/env python3
#
# MIT License
#
# Copyright (c) 2025 Konstantin Polevik
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
import argparse
import hashlib
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Test runner")
    parser.add_argument("-e", "--exec", required=True, help="")
    parser.add_argument("-i", "--input", required=True, help="")
    parser.add_argument("-c", "--checksum", required=True, help="")

    args = parser.parse_args()
    if not args:
        return 1

    result = subprocess.run([args.exec, args.input], check=True)
    wav_file = args.input + ".wav"

    with open(wav_file, "rb") as f:
        data = f.read()
        md5 = hashlib.md5(data).hexdigest()
        if args.checksum != md5:
            raise Exception(
                f"The MD5 hash of the WAV file ({md5}) doesn't match the reference value {args.checksum}."
            )

    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
