require 'optparse'
params = Hash[ARGV.getopts('', 'run:', 'configure:', 'make', 'remake:', 'profile:').map { |k, v| [k.to_sym, v] }]

def configure mode
  puts 'configuring...'
  option = "--quiet --disable-install-doc --disable-rubygems --disable-FEATURE --disable-dln --disable-pie --disable-rpath --disable-largefile"
  # option = "--quiet --disable-install-doc --disable-rubygems --disable-FEATURE --disable-pie --disable-rpath --disable-largefile"
  if mode == 'debug'
    `autoconf && ./configure #{option} --enable-debug-env`
  else
    `autoconf && ./configure #{option}`
  end
end

def make
  puts 'making...'
  `make -j`
end

def clean
  puts 'cleaning...'
  `make clean`
end

def run filepath
  `RUBY_JIT_DEBUG= ./ruby #{filepath}`
end

if params[:run]
  run params[:run]
elsif params[:configure]
  configure params[:configure]
elsif params[:make]
  make
elsif params[:remake]
  configure params[:remake]
  clean
  make
elsif params[:profile]
  if params[:profile] == 'view'
    `sudo perf report -g -G`
  elsif params[:profile] == 'perf'
    `RUBY_JIT_DEBUG= sudo perf record -a -g ./ruby #{params[:profile]}`
  else
    `gprof ./ruby | ./gprof2dot.py | dot -Tsvg -o output.svg`
  end
end
