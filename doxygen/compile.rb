# encodeing: utf-8
# This script requires "doxygen" and "graphviz"
#		Author:: Tatsuya Tanaka
#		Data:: 2015/6/9

require 'optparse'

def get_options
	args = {}
	OptionParser.new do |op|
		op.on('-s', '--setup', 'Setup necessary files') {|v| args[:s] = v}
		op.on('-f', '--figure', 'Generate figures') {|v| args[:f] = v}
		op.parse!(ARGV)
	end
	args
end

def replace_line filepath, re, &proc
	replace_lines filepath, { re => proc }
end

def replace_lines filepath, re_procs
	output = ''
	File.open(filepath, 'r') do |file|
		file.each_line do |line|
			re_procs.each do |re, proc|
				output +=
					if m = re.match(line)
						newline = proc.call(m)
						return unless newline
						newline
					else
						line
					end
			end
		end
	end
	File.write(filepath, output)
end

def update_ruby_version
	replace_line 'Doxyfile', /(PROJECT_BRIEF\s*=\s*)"(.*)"/ do |m|
		ver = `./ruby -v`.gsub(/\n/,'').chop
		"#{m[1]}\"#{ver}\"\n" unless m[2].to_s == ver
	end
end


def update_timestamp
	files = ['DOXYGENREADME.md', 'DOXYGENTERMS.md', 'DOXYGENLINKS.md', 'DOXYGENMICROSCOPE.md']
	files.each do |file|
		replace_line file, /(created at: )/ do |m|
			"#{m[1]}#{Time.now}\n"
		end
	end
end

def set_graph_showed yes_or_no
	re_procs = {
		/(HAVE_DOT\s*=\s*)/ => lambda {|m| "#{m[1]}#{yes_or_no}\n"},
	}
	replace_lines 'Doxyfile', re_procs
end




options = get_options

update_ruby_version
update_timestamp

set_graph_showed "YES" if options[:f]

`doxygen`

set_graph_showed "NO" if options[:f]

if options[:s]
	puts 'Generating necessary files...'
	`cp doxygen/customdoxygen.css html/`
end


