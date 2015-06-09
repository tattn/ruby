# encodeing: utf-8
# This script requires "doxygen" and "graphviz"
#		Author:: Tatsuya Tanaka
#		Data:: 2015/6/9

require 'optparse'

def get_options
	args = {}
	OptionParser.new do |op|
		op.on('-g', '--generate', 'Generate necessary files') {|v| args[:g] = v}
		op.parse!(ARGV)
	end
	args
end

def replace_line filepath, re, &proc
	output = ''
	File.open(filepath, 'r') do |file|
		file.each_line do |line|
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
	File.write(filepath, output)
end

def update_ruby_version
	replace_line 'Doxyfile', /(PROJECT_BRIEF\s*=\s*)"(.*)"/ do |m|
		ver = `./ruby -v`.chop
		"#{m[1]}\"#{ver}\"\n" unless m[2].to_s == ver
	end
end

def update_readme
	replace_line 'DOXYGENEXTRA.md', /(created at: )/ do |m|
		"#{m[1]}#{Time.now}\n"
	end
end

options = get_options

update_ruby_version
update_readme

`doxygen`

if options[:g]
	puts 'Generating necessary files...'
	`cp doxygen/customdoxygen.css html/`
end

