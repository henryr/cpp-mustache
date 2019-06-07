cc_library(
  name = "mustache",
  hdrs = ["mustache.h"],
  srcs = ["mustache.cc"],
  deps = ["@rapidjson//:rapidjson"],
  copts = ["-Wno-sign-compare"],
)

cc_test(
  name = "mustache-tests",
  srcs = ["mustache-tests.cc"],
  deps = [ "mustache", "@googletest//:gtest_main", "@rapidjson//:rapidjson" ],
  data = glob([ "test-templates/*" ])
)
