# Copyright (C) 2013-2016 Apple Inc. All rights reserved.
# Copyright (C) 2023 Igalia S.L. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# The BasePlan is also used in our self tests.
class BasePlan
    attr_reader :directory, :arguments, :family, :name, :outputHandler, :errorHandler, :additionalEnv, :index
    attr_accessor :retryParameters

    @@index = 0
    def initialize(directory, arguments, family, name, outputHandler, errorHandler, retryParameters)
        @directory = directory
        @arguments = arguments
        @family = family
        @name = name
        @outputHandler = outputHandler
        @errorHandler = errorHandler
        # A plan for which @retryParameters is not nil is being
        # treated as potentially flaky.
        @retryParameters = retryParameters
        @additionalEnv = []
        @index = @@index
        @@index += 1
    end
    def self.mock(family, name, retryParameters=nil)
        self.new("/none", [], family, name, nil, nil, retryParameters)
    end
    def self.create(directory, arguments, family, name, outputHandler, errorHandler)
        if $runCommandOptions[:crashOK]
            outputHandler = "noisy"
        end
        self.new(directory, arguments, family, name, outputHandler, errorHandler, $runCommandOptions[:flaky])
    end
    # We regularly place Plans in containers, but may modify @retryParameters
    # after the fact; only hash on @index instead.
    def hash
        @index
    end
    def to_s
        "#{@index}"
    end
end

class Plan < BasePlan
    def writeTestCase(filename)
        File.open(filename, "w") { |out|
            names = []
            ($envVars + additionalEnv).each { |var|
                next unless var.strip.size > 0
                fields = var.split("=")
                if fields.size != 2
                    raise "Couldn't parse env var `#{var}`"
                end
                names << fields[0]
                out.puts("#{fields[0]}=#{Shellwords.shellescape(fields[1])}")
            }
            # export_vars contains the name of each variable that is to be
            # exported; this is required to be able to dynamically generate the
            # variable names on POSIX sh.
            out.puts("EXPORT_VARS=#{Shellwords.shelljoin(names)}")
            out.puts("DIRECTORY=#{Shellwords.shellescape(@directory)}")
            out.puts("TESTNAME=#{Shellwords.shellescape(@name)}")
            out.puts("CMD=#{Shellwords.shellescape(Shellwords.shelljoin(@arguments))}")
            out.puts("OUTPUT_HANDLER=#{Shellwords.shellescape(@outputHandler)}")
            out.puts("ERROR_HANDLER=#{Shellwords.shellescape(@errorHandler)}")
        }
    end
end
