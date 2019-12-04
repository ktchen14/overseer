module Vision
  class System
  end

  class Device
    attr_reader :path

    def initialize(path)
      if path.start_with?('/sys')
        @path = path
      elsif path.start_with?('/')
        @path = "/sys#{path}"
      else
        @path = "/sys/#{path}"
      end
    end

    def driver
      File.basename(File.readlink(File.join(path, 'driver')))
    rescue Errno::ENOENT
      nil
    end
  end
end
