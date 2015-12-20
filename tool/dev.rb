case ARGV[0]
when 'run'
  if ARGV[1]
    `RUBY_JIT_DEBUG= ./ruby --disable-gems #{ARGV[1]}`
  else
    puts 'ruby devutils.rb run [script_path]'
  end
when 'configure'
  `autoconf && ./configure --disable-install-doc`
when 'make'
  `make -j`
when 'prof'
  if ARGV[1] == 'view'
    `perf report -g -G`
  else
    `RUBY_JIT_DEBUG= sudo perf record -a -g ./ruby --disable-gems #{ARGV[1]}`
  end
  # `rm perf.data`
end
