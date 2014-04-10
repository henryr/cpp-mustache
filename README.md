cpp-mustache
============

Quick'n'dirty Apache-licensed C++ implementation of a [Mustache](http://mustache.github.io/) template engine with support for [RapidJSON](https://code.google.com/p/rapidjson/).

*This is a work-in-progress:* after one day of work, cpp-mustache is now usable for my purposes, but may not be for yours. There are missing features (e.g. partials), and there are doubtless bugs (particularly when the template is malformed).

To use
======

Easiest way is to drop `mustache.{cc,h}` in your project. RapidJSON headers must be on the include path.

Then use in your code like:

    #include "mustache.h"
    stringstream ss;
    rapidjson::Document d;
    d.Parse<0>("{\"greeting\": \"hello world\"}");
    mustache::RenderTemplate("{{greeting}}", d, &ss);
    cout << ss.str() << endl;

To compile and run the tests
=============================

    cd thirdparty/gtest-1.7.0
    mkdir mybuild && cd mybuild
    cmake ../ && make -j8
    cd ../../
    cmake . && make -j8
    
Then run the tests:

    ./mustache-tests
    

