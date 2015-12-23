require "open3"

#==========================================
# http://qiita.com/fetaro/items/6a1ad6bf3c14470c949b
require 'fileutils'
require 'tempfile'
require 'tmpdir'
require 'timeout'

class Cmd
  @@timeout = 0
  def Cmd.timeout=(sec) ; @@timeout = sec ; end
  def Cmd.run(*cmd_array)
    raise(Exception,"Command string is nil") if cmd_array.size == 0
    stdout_file = Tempfile::new("#{$$}_light_cmd_tmpout",Dir.tmpdir)
    stderr_file = Tempfile::new("#{$$}_light_cmd_tmperr",Dir.tmpdir)
    pid = spawn(*cmd_array, :out => [stdout_file,"w"],:err => [stderr_file,"w"])
    status = nil
    if @@timeout != 0
      begin
        timeout(@@timeout) do
          status = Process.waitpid2(pid)[1] >> 8
        end
      rescue Timeout::Error => e
        begin
          # Process.kill('SIGINT', pid)
          Process.kill('SIGKILL', pid)
          `kill -9 #{pid}`
          `kill -KILL #{pid}`
          sleep(1)
          raise(Exception,"Timeout #{@@timeout.to_s} sec. Kill process : PID=#{pid}" )
        rescue
          raise(Exception,"Timeout #{@@timeout.to_s} sec. Fail to kill process : PID=#{pid}" )
        end
      end
    else
      status = Process.waitpid2(pid)[1] >> 8
    end
    return  Result.new(stdout_file,stderr_file,status,cmd_array)
  end

  class Result
    attr_reader :stdout_file, :stderr_file, :status, :cmd_array
    def initialize(stdout_file, stderr_file, status, cmd_array)
      @stdout_file = stdout_file
      @stderr_file = stderr_file
      @status = status
      @cmd_array = cmd_array
    end
    def stdout  ; File.read(@stdout_file) ; end
    def stderr  ; File.read(@stderr_file) ; end
    def to_h    ; {:cmd_str => cmd_str,:stdout => stdout, :stderr => stderr, :status => status} ; end
    def cmd_str ; @cmd_array.join(" ") ; end
    def to_s    ;to_h.to_s ; end
  end
  class Exception < StandardError ; end
end

class String
  def colorize(color_code)
    "\e[#{color_code}m#{self}\e[0m"
  end

  def red
    colorize(31)
  end

  def green
    colorize(32)
  end

  def yellow
    colorize(33)
  end

  def pink
    colorize(35)
  end
end

#==========================================



RUBY = '~/mywork/orig_ruby/ruby --disable-gems'
MYRUBY = '~/mywork/myruby/ruby'
BENCHMARK =  File::expand_path('~/mywork/myruby/benchmark')

def run_benchmark filename, ruby = MYRUBY
  benchmark = "RUBY_JIT_DEBUG= time #{ruby} #{BENCHMARK}/#{filename}"

  Cmd.timeout = 10
  result = Cmd.run(benchmark)
  return [result.stdout, result.stderr, result.status]

rescue Cmd::Exception
  return ["", "timeout", -1]
end

results = []
benchmarks = []
ignored_benchmarks = []
Dir::glob("#{BENCHMARK}/*.rb").each do |b|
  b = File.basename(b)
  if !%w(bm_so_lists.rb
         bm_app_pentomiso.rb
         bm_so_sieve.rb
         bm_vm1_gc_short_lived.rb
         bm_vm1_gc_short_with_symbol.rb
         bm_so_fasta.rb
         bm_so_nbody.rb
         bm_app_tak.rb
         bm_vm1_attr_ivar.rb
         bm_vm1_lvar_set.rb
         bm_vm2_bighash.rb
         bm_vm2_bigarray.rb
         bm_app_pentomino.rb
         bm_so_reverse_complement.rb
         bm_vm1_gc_short_with_long.rb

         driver.rb
         make_fasta.output.rb
         prepare_so_count_words.rb
         prepare_so_k_nucletide.rb
         prepare_so_reverse_complement.rb
         report.rb
         run.rb
         runc.rb
         ).include? b
    benchmarks.push b
  else
    ignored_benchmarks.push b
  end
end


[MYRUBY, RUBY].each_with_index do |ruby, index|
  results.push []
  benchmarks.each_with_index do |filename, index2|
    print "#{filename}: ".green if ruby == MYRUBY
    print "#{filename}: ".pink if ruby == RUBY

    if ruby == RUBY
      if results[0][index2] == -1
        puts "pass"
        results[index].push -1
        next
      end
    end

    out, err, status = run_benchmark(filename, ruby)

    if status == 0
      err.match(/(.*)user.*/) do |md|
        puts "succeeded! [time: #{md[1]}]"
        results[index].push md[1].to_f
      end
    else
      if err == "timeout"
        puts "failed[#{status}] - timeout".red
      else
        puts "failed[#{status}] - SEGV".red
      end
      results[index].push -1
    end
    sleep 1
  end
end

data = []

puts "================ RESULT ================="
benchmarks.each_with_index do |benchmark, index|
  result1 = results[0][index]
  result2 = results[1][index]

  if result1 == -1 or result2 == -1
    puts "#{benchmark}, *"
    data.push 0
  else
    per = result2 / result1
    puts "#{benchmark}, #{per} = #{result2} / #{result1}"
    data.push per
  end
end

File.open('result.csv', 'w') do |f|
  f.puts "================== MYRUBY =================="
  f.puts results[0].join(",")
  f.puts "================== ORIGINAL RUBY =================="
  f.puts results[1].join(",")
  f.puts "================== COMPARISON FOR EXCEL =================="
  data.each_with_index do |d, index|
    if results[0][index] != -1
      f.puts "#{benchmarks[index]}, #{d}, #{results[0][index]}, #{results[1][index]}"
    end
  end
end

File.open('failed.txt', 'w') do |f|
  f.puts "================== SEGV or TIMEOUT =================="
  data.each_with_index do |d, index|
    if results[0][index] == -1
      f.puts "#{benchmarks[index]}"
    end
  end
  f.puts "================== IGNORED =================="
  ignored_benchmarks.each do |b|
    f.puts b
  end
end

