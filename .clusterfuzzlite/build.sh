cd $SRC/2.Fuzzing_integration

$CXX $CXXFLAGS -std=c++17 -I lib \
    fuzzing/fuzz_deserialize.cpp lib/protocol.cpp \
    -o $OUT/fuzz_deserialize $LIB_FUZZING_ENGINE

$CXX $CXXFLAGS -std=c++17 -I lib \
    fuzzing/fuzz_roundtrip.cpp lib/protocol.cpp \
    -o $OUT/fuzz_roundtrip $LIB_FUZZING_ENGINE


if [ -d "$SRC/2.Fuzzing_integration/corpus/deserialize" ]; then
  (cd $SRC/2.Fuzzing_integration/corpus/deserialize && zip -r "$OUT/fuzz_deserialize_seed_corpus.zip" .)
fi
if [ -d "$SRC/2.Fuzzing_integration/corpus/roundtrip" ]; then
  (cd $SRC/2.Fuzzing_integration/corpus/roundtrip && zip -r "$OUT/fuzz_roundtrip_seed_corpus.zip" .)
fi