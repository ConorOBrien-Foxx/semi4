# ;;;;

`;;;;` (pronounced `semi4`) is a procedural esoteric language that compiles to C.

```
semi4.exe <file-name> <output-name>
```

Compile source code with `gcc -g -Wall semi4.c -o semi4`.

## Language Description

The language operates on 64 registers (`a-zA-Z0-9_$`), which are either typed as strings or integers.

Programs consist of a data section, followed by alternating code and looped code sections. The data section is a manifest of variables and their initial values.

Instructions are defined to be a register name, followed by the specific command name, followed by the appropriate number of arguments.

## Example

```
'data section
x 'register x
n 'is a number
0 'and is initialized to 0

y n2    'register y is a number initialized to 2
z n-5   'z = -5

'H is a string with an initial size of 50
H s50.
'J is a string, initially "Hello!"
J s6Hello! 
'strings must be terminated by `.` if it has blank cells
K s100Done!.
'the empty string
E s0

; 'code section 1
Jp      'print J (Hello!)
x+1 x   'increment x and store in x
xp      'print x (1)

;z 'looped code section 1 (while z not 0)
    zp      'print z
    z+1 z   'increment z
'prints -5, -4, -3, -2, -1
; 'code section 2
Kp      'print K (Done!)
```

Without comments:
```
xn0yn2zn-5Hs50.Js6Hello!Ks100Done!.Es0;Jpx+1xxp;zzpz+1z;Kp
```

Overall output:
```
Hello!
1
-5
-4
-3
-2
-1
Done!
```