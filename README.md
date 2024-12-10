# Performance results

To test: `make regression`
To benchmark: `make benchmark`

```
lamac -b performance/Sort.lama
mv Sort.bc build/Sort.bc
build/analyzer build/Sort.bc verify
verification took 0.000000s
execution without checks took 1.886000s
build/analyzer build/Sort.bc runtime
execution with checks took 2.001000s
cat empty | `which time` -f "./lamac -i \t%U" lamac -i performance/Sort.lama
./lamac -i      5.52
cat empty | `which time` -f "./lamac -s \t%U" lamac -s performance/Sort.lama
./lamac -s      2.11
```