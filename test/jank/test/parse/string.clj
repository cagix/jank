(ns jank.test.parse.string
  (:require [clojure.test :refer :all]
            [jank.test.parse.all :as util]))

(deftest string
  (doseq [file ["escape/pass-lots-of-unescaped-closes"
                "escape/pass-unescaped-both"
                "escape/pass-unescaped-close"
                "escape/pass-unescaped-open"
                "escape/pass-escape-both"
                "escape/pass-escape-close"
                "escape/pass-escape-open"]]
    (util/test-file (str "test/parse/string/" file))))