# 使い捨てベンチマーク評価ツール

require "open3"
require 'optparse'

params = Hash[ARGV.getopts('', 'run', 'compare').map { |k, v| [k.to_sym, v] }]

#==========================================
# Thanks to http://qiita.com/fetaro/items/6a1ad6bf3c14470c949b
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



#============== Configuration =============
RUBY = '~/mywork/orig_ruby/ruby --disable-gems'
MYRUBY = '~/mywork/myruby/ruby'
BENCHMARK =  File::expand_path('~/mywork/myruby/benchmark')
#==========================================

def run_benchmark filename, ruby = MYRUBY
  benchmark = "RUBY_JIT_DEBUG= time #{ruby} #{BENCHMARK}/#{filename}"

  Cmd.timeout = 10
  result = Cmd.run(benchmark)
  return [result.stdout, result.stderr, result.status]

rescue Cmd::Exception
  return ["", "timeout", -1]
end

benchmarks = []
ignored_benchmarks = []
Dir::glob("#{BENCHMARK}/*.rb").each do |b|
  b = File.basename(b)
  # timeout
  # need an input file
  # SEGV
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
         bm_so_k_nucleotide.rb

         driver.rb
         make_fasta.output.rb
         prepare_so_count_words.rb
         prepare_so_k_nucletide.rb
         prepare_so_reverse_complement.rb
         report.rb
         run.rb
         runc.rb
         make_fasta_output.rb
         prepare_so_k_nucleotide.rb

         bm_app_erb.rb
         bm_vm2_method.rb
         bm_vm1_gc_short_with_complex_long.rb
         bm_vm_thread_mutex1.rb
         bm_so_nsieve_bits.rb
         bm_vm2_mutex.rb
         bm_vm_thread_pipe.rb
         bm_so_array.rb
         bm_hash_aref_str.rb
         bm_so_pidigits.rb
         bm_app_tarai.rb
         bm_hash_aref_str.rb
         bm_so_pidigits.rb
         bm_app_tarai.rb
         bm_vm1_block.rb
         bm_so_binary_trees.rb
         bm_so_object.rb
         bm_vm1_yield.rb
         bm_vm2_case.rb
         bm_vm2_super.rb
         bm_vm3_gc.rb
         bm_app_factorial.rb
         bm_array_shift.rb
         bm_vm3_clearmethodcache.rb
         bm_vm_thread_close.rb
         bm_vm_thread_create_join.rb
         bm_app_aobench.rb
         bm_vm2_struct_big_aset.rb
         bm_vm1_lvar_init.rb
         bm_vm_thread_pass.rb
         bm_vm2_proc.rb
         bm_vm2_defined_method.rb
         bm_so_partial_sums.rb
         bm_vm1_simplereturn.rb
         bm_vm2_poly_method_ov.rb
         bm_vm1_neq.rb
         bm_hash_shift.rb
         bm_vm2_struct_small_aset.rb
         bm_io_select3.rb
         bm_vm1_float_simple.rb
         bm_vm2_struct_big_aref_lo.rb
         bm_vm2_eval.rb
         bm_so_exception.rb
         bm_vm2_zsuper.rb
         bm_vm2_raise2.rb
         bm_so_nsieve.rb
         bm_vm2_method_with_block.rb
         bm_vm_thread_queue.rb
         bm_vm_thread_alive_check1.rb
         bm_hash_aref_miss.rb
         bm_so_fannkuch.rb
         bm_app_lc_fizzbuzz.rb
         bm_io_file_write.rb
         bm_vm2_poly_method.rb
         bm_vm2_struct_big_aref_hi.rb
         bm_so_spectralnorm.rb
         bm_app_raise.rb
         bm_io_file_read.rb
         bm_vm_thread_mutex2.rb
         bm_vm2_send.rb
         bm_vm2_method_missing.rb
         bm_app_answer.rb
         bm_vm2_raise1.rb
         bm_so_meteor_contest.rb
         bm_so_mandelbrot.rb
         bm_so_random.rb
         bm_vm_thread_mutex3.rb
         bm_vm1_gc_wb_obj_promoted.rb
         bm_so_concatenate.rb
         bm_vm2_unif1.rb
         bm_app_uri.rb
         bm_securerandom.rb
         bm_so_matrix.rb
         bm_vm1_gc_wb_ary_promoted.rb
         bm_so_ackermann.rb
    ).include? b
    benchmarks.push b
  else
    ignored_benchmarks.push b
  end
end


if params[:run]
  results = []
  benchmarks.each_with_index do |filename, index|
    result = {}
    [MYRUBY, RUBY].each_with_index do |ruby, index2|
      out, err, status = run_benchmark(filename, ruby)

      if status == 0
        err.match(/(.*)user.*/) do |md|
          result[ruby] = md[1]
        end
      else
        result[:err] = {status: status, type: err == "timeout" ? "timeout" : "SEGV"}
        result[MYRUBY] = result[RUBY] = -1
        break
      end
    end
    if result[MYRUBY] == -1
      puts "#{result[:err][:type].red}[#{result[:err][:status]}] @ #{filename}"
    else
      myruby = result[MYRUBY]
      ruby = result[RUBY]
      result[:compare] = ruby.to_f / myruby.to_f
      if myruby.to_f <= ruby.to_f
        emotion = "HAPPY :-)".green
        myruby = myruby.green
      else
        if myruby.to_f - ruby.to_f <= 0.1
          emotion = " SAFE :-|".yellow
          ruby = ruby.yellow
        else
          emotion = " SAD  ;-(".red
          ruby = ruby.red
        end
      end
      printf "%s [%s, %s](%f) @ %s\n", emotion, myruby, ruby, result[:compare], filename
    end
    results.push result
  end

  File.open('result.csv', 'w') do |f|
    f.puts "================== COMPARISON FOR EXCEL =================="
    results.each_with_index do |result, index|
      # if result[MYRUBY] != -1
        f.puts "#{benchmarks[index]}, #{result[:compare]}, #{result[MYRUBY]}, #{result[RUBY]}"
      # end
    end
  end

  File.open('failed.txt', 'w') do |f|
    f.puts "================== SEGV or TIMEOUT =================="
    results.each_with_index do |result, index|
      if result[MYRUBY] == -1
        f.puts "#{benchmarks[index]}"
      end
    end
    f.puts "================== IGNORED =================="
    ignored_benchmarks.each do |b|
      f.puts b
    end
  end

elsif params[:compare]
  File.open('result.csv') do |new_f|
    File.open('old_result.csv') do |old_f|
      new_benchmarks = new_f.read.split("\n")
      old_benchmarks = old_f.read.split("\n")
      new_benchmarks.shift 1
      old_benchmarks.shift 1
    
      new_benchmarks.zip(old_benchmarks).each do |new, old|
        new = new.split(",")
        old = old.split(",")
        if new[2].to_i == -1 or old[2].to_i == -1
          puts "pass"
        else
          result = new[1].to_f - old[1].to_f
          if result == 0
            result = result.to_s
          elsif result < 0
            if result >= -0.05
              result = result.to_s.yellow
            else
              result = result.to_s.red
            end
          elsif result > 0
            result = result.to_s.green
          end
          puts "#{result} @ #{new[0]}"
        end
      end
    end
  end
end
