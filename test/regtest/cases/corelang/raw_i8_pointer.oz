(block
  (type Inner <struct (value i64)>)
  (type Outer <struct (inner <named Inner>) (extra i64)>)

  (fun <main> ()
    (block
      (var pointers <array <ptr i8> 1> [0 1])
      (= pointers [0] (cast (: 17 i64) <ptr i8>))
      (= pointers [1] (cast (: 29 i64) <ptr i8>))
      (output (cast (index pointers (: 0 i64)) i64) " "
              (cast (index pointers (: 1 i64)) i64) "\n")
      (output
        (call sum_outer
          (: (struct
               ((inner (: (struct ((value (: 40 i64)))) <named Inner>))
                (extra (: 2 i64))))
             <named Outer>))
        "\n")))

  (fun sum_outer ((var value <named Outer>)) -> i64
    (block
      (return (+ (field (field value inner) value)
                 (field value extra))))))
