# std::cout is out, why iostreams must go

The slides to the talk I gave at the [Berlin C++ Meetup on Nov 17 2015](http://www.meetup.com/de/berlincplusplus/events/226478618/).

Folder `slides` contains the slides as an HTML file. Open the file in your browser and hit 'P' to see the presenter notes. I've used [remark.js](http://remarkjs.com) to create the slides. 

Folder `benchmark` contains the source code with a small (entirely non-scientific) I/O benchmark I made. It compares the performance of `std::iostreams`, `std::FILE*` and raw I/O using file handles in three scenarios: Writing chunks of 1M at a time, writing a single character at a time and formatted output with a few more characters at a time. I've used the excellent [nonius benchmarking framwork](https://nonius.io) for the actual measurements. Compile the source with 

		c++ -std=c++14 -Iext -I/path_to_/boost_1_59_0/ iobench.cpp 