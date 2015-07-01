TEMPLATE = 'cruby_type2llvm.c'
TEMPORARY = File.basename(TEMPLATE, '.c') + '.ll'
OUTPUT = 'jit_typedef.inc'

INCLUDE_DIRS = [
	'../../',
	'../../include',
	'../../include/ruby',
	'/usr/local/include/ruby-2.3.0/x86_64-darwin14/',
]

OPTION_INCLUDE = INCLUDE_DIRS.map {|dir| "-I#{dir}"}.join(" ")

`clang -cc1 #{TEMPLATE} -emit-llvm #{OPTION_INCLUDE}`

File.open OUTPUT, 'w' do |output|
	output.puts 'R"STR('
	File.open TEMPORARY, 'r' do |llvmir|
		llvmir.each do|line|
			if line[0] == '%' or line.start_with?('declare')
				output.puts line
			end
		end
	end
	output.puts ')STR";'
end

