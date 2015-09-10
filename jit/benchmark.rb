require "open3"

if ARGV[0] == 'all'
	benchmarks = `ls benchmark`.split("\n").grep(/bm.*\.rb/)
else
	benchmarks = [
		'bm_loop_for.rb',
		'bm_loop_times.rb',
	]
end


cmd = 'JIT_RUBY_DEBUG= ./ruby benchmark/'

benchmarks.each do |b|
	puts b
	stdout, stderr, status = Open3.capture3 cmd + b
	p stdout
	p stderr
	p status
end
