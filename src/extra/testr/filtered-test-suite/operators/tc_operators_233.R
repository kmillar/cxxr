expected <- eval(parse(text="structure(c(1, 0, -1, 0.5, -0.5, NA, NA, NA, 0), .Dim = c(3L, 3L))"));      
test(id=0, code={      
argv <- eval(parse(text="list(structure(c(2, 1, 0, 1, 0, NA, NA, NA, 0), .Dim = c(3L, 3L)), structure(c(1, 1, 1, 0.5, 0.5, 0.5, 0, 0, 0), .Dim = c(3L, 3L)))"));      
do.call(`-`, argv);      
}, o=expected);      

