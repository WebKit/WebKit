#!/usr/bin/ruby
# Gives you information about the most recent crash for each application
# that has crashed within the last 60 minutes

$LogDir=ENV['HOME'] + '/Library/Logs/CrashReporter'
$Min=60
$StackCount=5

files=`find #{$LogDir} -mmin -#{$Min} -type f | grep -v synergy`
files.each { |filename|
    filename.chop!
    record = 0
    date=''
    stackTrace = []

    File.open(filename).readlines.each { |line|
        #puts line

        if line =~ /^Date.*(200.*)/
            date = $1
        end

        if line =~ /^Thread \d+ Crashed/
            record = 1
            # reset the stack trace
            stackTrace = []
        end

        if record
            stackTrace << line
            record = record + 1

            # stop recording after $StackCount lines
            if record > ($StackCount + 2)
                record = nil
            end
        end
    }

    puts File.basename(filename) + " - " + date
    puts "==================================================="
    stackTrace.each { |line|
        puts line
    }
    puts ""
}


