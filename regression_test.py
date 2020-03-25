#!/usr/bin/python

import re
import subprocess
import sys


GET_DATA = re.compile(r'= (\d+) calls, length (-?\d+:.*)')
BINARY = './antares-3'


def Check(n, expected, stdout):
  line = stdout.readline()
  while not GET_DATA.match(line):
    line = stdout.readline()
  re_groups = GET_DATA.match(line.strip())
  calls = re_groups.group(1)
  print calls
  group = re_groups.group(2)
  length1, moves1 = group.split(':')
  length0, moves0 = expected.strip().split(':')
  if length0 != length1 or set(moves0) != set(moves1):
    print 'test %d: got %s; expected %s' % (n, group, expected)
    return 1
  else:
    return 0


def main():
  if subprocess.call(
      ['make', BINARY, 'DEBUG=1'],
      stdout=subprocess.PIPE) != 0:
    sys.exit('\033[31mMake failed\033[0m\n')
  p = subprocess.Popen(
      BINARY, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  f = open('regression_tests.txt')
  n = 0
  failed_tests = 0
  for line in f:
    if not line.strip():
      p.stdin.write('clearboard\n')
      p.stdin.flush()
    elif line.startswith('>'):
      n += 1
      failed_tests += Check(n, line[2:], p.stdout)
    else:
      p.stdin.write(line)
      p.stdin.flush()
  f.close()
  if failed_tests == 0:
    sys.stderr.write('\033[32mSUCCESS\033[0m\n')
  else:
    sys.exit('\033[31m%d/%d FAILURES\033[0m\n' % (failed_tests, n))


if __name__ == '__main__':
  main()
