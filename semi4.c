#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define OUTNAME             "temp.c"
#define COMPILE(name, out)  "gcc " name " -o " out
#define RUN(name, out)       out
#ifdef _WIN32
#define REMOVE(name)        "del " name
#else
#define REMOVE(name)        "rm " name
#endif

/*
 * 64 registers (a-zA-Z0-9_$)
 * commands are defined as a register name, followed by a command name,
 * followed by 0 or more arguments
 * 
 * semicolons separate sections of the program
 * section 1: data
 *  string of /(([a-zA-Z])([sn]))+?/
 * section 2: init code
 * section 3: loop code
 * section 4: post code
 * section 2N-1: loop code
 * section 2N: post code
 * ...
 */

#define OUTPUT(str) fprintf(compileFile, "%s", str)
#define OUTPUTF(str, ...) fprintf(compileFile, str, __VA_ARGS__)

#define FAIL(code, msg, ...) {\
    fprintf(stderr, "(%s:%i) Fatal Error: " msg "\n", __FILE__, __LINE__, __VA_ARGS__);\
    return code;\
}
#define FAILE(code, msg, ...) {\
    fprintf(stderr, "(%s:%i) Fatal Error: " msg "\n", __FILE__, __LINE__, __VA_ARGS__);\
    exit(code);\
}

#define FAIL_TODO() FAIL(420, "TODO: Implement this feature (%s:%i)", __FILE__, __LINE__)
#define FAILE_TODO() FAILE(420, "TODO: Implement this feature (%s:%i)", __FILE__, __LINE__)
#define DTYPE_SNAME(dt) (dt == STRING ? "string" : dt == NUMBER ? "numeric" : "undefined")
#define FAIL_UNEXPECTED(c, actual, expected) \
    FAILE(9, "Expected %s register `%c`, got %s", DTYPE_SNAME(expected), c, DTYPE_SNAME(actual))

#define NBUF_MAX (10)

enum PMODE { SINGLE, LOOP };
enum DTYPE { UNDEFINED, STRING, NUMBER };
 
static char* boilerplate[2] = {
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include \"s4str.h\"\n"
    "int main(int argc, char** argv) {\n"
    "FILE* istream = stdin; FILE* ostream = stdout;\n"
    "int _; s4str* $ = s4str_new(1);\n"
    ,
    "}\n"
};

int isRegName(int c) {
    return isalpha(c) || isdigit(c) || c == '_' || c == '$';
}

int main(int argc, char** argv) {
    if(argc < 2) {
        FAIL(1, "%s", "Expected file name to interpret.");
    }
    char* outputName;
    if(argc < 3) {
        outputName = "t";
    }
    else {
        outputName = argv[2];
        for(char* c = outputName; *c; c++) {
            if(!isalpha(*c) && *c != '-' && *c != '.') {
                FAIL(10, "Invalid output name character detected: %c\n", *c);
            }
        }
    }
    int debug = 0;
    for(int i = 3; i < argc; i++) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'd': debug = 1; break;
                default: fprintf(stderr, "Warning: Unknown flag `%s`\n", argv[i]); break;
            }
        }
    }
    
    FILE* codeFile = fopen(argv[1], "r");
    FILE* compileFile = fopen(OUTNAME, "w");
    
    OUTPUT(boilerplate[0]);
    
    // parse input program
    int cur;
    #define MAXBUFSIZE (8)
    int buffer[MAXBUFSIZE];
    int bufferSize = 0;
    void nextSkipSpace(int* cref) {
        int val = 0;
        int inComment = 0;
        while((!feof(codeFile) || bufferSize) && (val == 0 || isspace(val) || inComment)) {
            if(!bufferSize) {
                val = fgetc(codeFile);
            }
            else {
                val = buffer[--bufferSize];
            }
            if(val == '\'') {
                inComment = 1;
            }
            else if(val == '\n' && inComment) {
                inComment = 0;
                val = 0;
            }
        }
        *cref = val;
    }
    
    void next(int* cref) {
        int val = 0;
        while((!feof(codeFile) || bufferSize) && val == 0) {
            if(!bufferSize) {
                val = fgetc(codeFile);
            }
            else {
                val = buffer[--bufferSize];
            }
        }
        *cref = val;
    }
    
    void unbuf(int bc) {
        if(bufferSize == MAXBUFSIZE) {
            FAILE(6, "Un-Buffer overflow (max capacity %i)", MAXBUFSIZE);
        }
        buffer[bufferSize++] = bc;
    }
    #define MODE_COUNT (26 + 26 + 10 + 2)
    enum DTYPE modes[MODE_COUNT] = { UNDEFINED };
    int chrid(int c) {
        if('a' <= c && c <= 'z') {
            return c - 'a';
        }
        else if('A' <= c && c <= 'Z') {
            return c - 'A' + 26;
        }
        else if('0' <= c && c <= '9') {
            return c - '0' + 26 + 26;
        }
        else if(c == '$') {
            return 26 + 26 + 10 + 0;
        }
        else if(c == '_') {
            return 26 + 26 + 10 + 1;
        }
        else {
            FAILE(2, "Expected register name (got `%c`)", c);
        }
    }
    int idchr(int id) {
        if(id < 26) {
            return 'a' + id;
        }
        else if(id < 26 + 26) {
            return 'A' + (id - 26);
        }
        else if(id < 26 + 26 + 10) {
            return '0' + (id - 26 - 26);
        }
        else if(id == 26 + 26 + 10 + 0) {
            return '$';
        }
        else if(id == 26 + 26 + 10 + 1) {
            return '_';
        }
        else {
            return -1;
        }
    }
    for(int c = '0'; c <= '9'; c++) {
        modes[chrid(c)] = NUMBER;
    }
    modes[chrid('_')] = NUMBER;
    modes[chrid('$')] = STRING;
    
    enum DTYPE getMode(int reg) {
        enum DTYPE t = modes[chrid(reg)];
        if(t == UNDEFINED) {
            FAILE(8, "Undeclared register `%c`", reg);
        }
        return t;
    }
    // parse data section
    while(!feof(codeFile)) {
        nextSkipSpace(&cur);
        if(feof(codeFile) || cur == ';') break;
        // if(!isRegName(cur)) {
        // we want only alphabetic registers in the data section
        if(!isalpha(cur)) {
            FAIL(2, "Expected register name (got `%c`)", cur);
        }
        char reg = cur;
        nextSkipSpace(&cur);
        char mode = cur;
        if(mode != 's' && mode != 'n') {
            FAIL(3, "Expected mode 's' or 'n' (got `%c`)", cur);
        }
        // read number
        char nbuf[NBUF_MAX + 1];
        int nptr = 0;
        int nstart = 0;
        // optional negative sign
        nextSkipSpace(&cur);
        if(!feof(codeFile)) {
            if(cur == '-') {
                nbuf[nptr++] = cur;
                nstart++;
            }
            else {
                unbuf(cur);
            }
        }
        while(!feof(codeFile)) {
            nextSkipSpace(&cur);
            if(feof(codeFile) || cur == ';') break;
            if(!isdigit(cur)) {
                if(nptr == nstart) {
                    FAIL(4, "Expected at least 1 digit after declaration (got `%c`)", cur);
                }
                else {
                    break;
                }
            }
            if(nptr == NBUF_MAX) {
                FAIL(5, "Postfix number cannot exceed %i digits", NBUF_MAX);
            }
            nbuf[nptr++] = cur;
        }
        unbuf(cur);
        nbuf[nptr] = '\0';
        if(mode == 's') {
            int val = atoi(nbuf);
            // TODO: assert val >= 0
            OUTPUTF("s4str* %c = s4str_new(%i);\n", reg, val + 1);
            for(int ctr = 0; ctr < val; ctr++) {
                next(&cur);
                if(feof(codeFile) || cur == '.') {
                    break;
                }
                OUTPUTF("s4str_set(%c, %i, %i); ", reg, ctr, cur);
            }
            OUTPUT("\n");
            // below line unnecessary since calloc is called
            // OUTPUTF("%c[%i] = 0;\n", reg, val);
            // OUTPUTF("size_t %c_size = %i;\n", reg, val);
            modes[chrid(reg)] = STRING;
        }
        else if(mode == 'n') {
            OUTPUTF("int %c = %s;\n", reg, nbuf);
            modes[chrid(reg)] = NUMBER;
        }
    }
    
    // -- parse code -- //
    
    void readRegister(int* reg, enum DTYPE expected) {
        nextSkipSpace(&cur);
        *reg = cur;
        enum DTYPE r2type = getMode(cur);
        if(r2type != expected) {
            FAIL_UNEXPECTED(cur, r2type, expected);
        }
    }
    
    // read a register but allow for continuation
    void readRegisterCons(int* reg, enum DTYPE expected) {
        nextSkipSpace(&cur);
        *reg = cur;
        if(!isRegName(cur)) {
            unbuf(cur);
            switch(expected) {
                case UNDEFINED: FAILE_TODO(); break;
                case STRING: *reg = '$'; unbuf(*reg); break;
                case NUMBER: *reg = '_'; unbuf(*reg); break;
            }
        }
        else {
            enum DTYPE r2type = getMode(cur);
            if(r2type != expected) {
                FAIL_UNEXPECTED(cur, r2type, expected);
            }
        }
    }
    
    int expectingClose = 0;
    enum PMODE mode = SINGLE;
    while(1) {
        nextSkipSpace(&cur);
        if(feof(codeFile)) break;
        if(cur == ';') {
            if(mode == LOOP) {
                OUTPUT("}\n");
            }
            mode = mode == SINGLE ? LOOP : SINGLE;
            if(mode == LOOP) {
                // nextSkipSpace(&cur);
                readRegister(&cur, NUMBER);
                OUTPUTF("while(%c) {\n", cur);
            }
        }
        else if(cur == '.') {
            if(expectingClose == 0) {
                FAIL(11, "%s", "Unexpected closer `.`");
            }
            OUTPUT("}\n");
            expectingClose--;
        }
        else if(cur == ':') {
            if(expectingClose == 0) {
                FAIL(11, "%s", "Unexpected join-closer `:`");
            }
            OUTPUT("} else {\n");
        }
        else if(!isRegName(cur)) {
            FAIL(2, "Expected register name (got `%c`)", cur);
        }
        else {
            char reg = cur;
            enum DTYPE rtype = getMode(reg);
            nextSkipSpace(&cur);
            char cmd = cur;
            switch(cmd) {
                // binary math operators
                case '+':
                case '-':
                case '*':
                case '/':
                case '%':
                case '<':
                case '>':
                case '&':
                case '|':
                case '=':
                case '^': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            if(cmd == '+') {
                                int rhs, out;
                                nextSkipSpace(&rhs);
                                enum DTYPE rhstype = getMode(rhs);
                                readRegisterCons(&out, STRING);
                                if(out != reg) {
                                    OUTPUTF("s4str_copyTo(%c, %c);\n", out, reg);
                                }
                                if(rhstype == STRING) {
                                    OUTPUTF("s4str_appendString(%c, %c);\n", out, rhs);
                                }
                                else {
                                    OUTPUTF("s4str_appendChar(%c, %c);\n", out, rhs);
                                }
                            }
                            else {
                                FAIL_UNEXPECTED(reg, rtype, NUMBER);
                            }
                            break;
                        }
                        case NUMBER: {
                            int rhs, out;
                            readRegister(&rhs, NUMBER);
                            readRegisterCons(&out, NUMBER);
                            if(cmd == '=') {
                                OUTPUTF("%c = %c == %c;\n", out, reg, rhs);
                            }
                            else {
                                OUTPUTF("%c = %c %c %c;\n", out, reg, cmd, rhs);
                            }
                            break;
                        }
                    }
                    break;
                }
                // unary operators
                case '~':
                case '_':
                case '!': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING:
                            FAIL_UNEXPECTED(reg, rtype, NUMBER);
                            break;
                        case NUMBER: {
                            int out;
                            readRegisterCons(&out, NUMBER);
                            OUTPUTF("%c = %c%c;\n", out, cmd, reg);
                            break;
                        }
                    }
                    break;
                }
                
                // assign
                case '$': {
                    int other;
                    readRegister(&other, rtype);
                    OUTPUTF("%c = %c;\n", reg, other);
                    break;
                }
                
                // charat
                case '@': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            int index, out;
                            readRegister(&index, NUMBER);
                            readRegisterCons(&out, NUMBER);
                            OUTPUTF("%c = s4str_get(%c, %c);\n", out, reg, index);
                            break;
                        }
                        case NUMBER:
                            FAIL_UNEXPECTED(reg, rtype, STRING);
                            break;
                    }
                    break;
                }
                
                // charset
                case '#': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            int index, value;
                            readRegister(&index, NUMBER);
                            readRegister(&value, NUMBER);
                            OUTPUTF("s4str_set(%c, %c, %c);\n", reg, index, value);
                            break;
                        }
                        case NUMBER:
                            FAIL_UNEXPECTED(reg, rtype, STRING);
                            break;
                    }
                    break;
                }
                
                // arg
                case 'a': {
                    int index;
                    readRegister(&index, NUMBER);
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            OUTPUTF("s4str_free(%c);\n", reg);
                            OUTPUTF("%c = s4str_from(argv[%c]);\n", reg, index);
                            break;
                        }
                        case NUMBER: {
                            FAIL_TODO();
                            break;
                        }
                    }
                    break;
                }
                
                // putchar
                case 'c': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING:
                            OUTPUTF("fprintf(ostream, \"%%s\", (char*) %c->data);\n", reg);
                            break;
                        case NUMBER: {
                            OUTPUTF("fputc(ostream, %c);\n", reg);
                            break;
                        }
                    }
                    break;
                }
                
                // debug
                case 'd': {
                    OUTPUTF("fprintf(ostream, \"%%s\", \"REGISTER '%c' = \");\n", reg);
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: OUTPUTF("s4str_puts_to(%c, ostream);\n", reg); break;
                        case NUMBER: OUTPUTF("fprintf(stderr, \"%%i\\n\", %c);\n", reg); break;
                    }
                    break;
                }
                
                // exit
                case 'e': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: FAIL_TODO(); break;
                        case NUMBER: OUTPUTF("return %c;\n", reg); break;
                    }
                    break;
                }
                
                // input file stream
                case 'f': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            int mode;
                            readRegister(&mode, NUMBER);
                            OUTPUTF("if(%c) {\n", mode);
                            OUTPUTF("istream = fopen(%c->data, fileModeNumber(%c));\n", reg, mode);
                            OUTPUT("} else {\n");
                            OUTPUT("fclose(istream);\nistream = stdin;\n");
                            OUTPUT("}\n");
                            break;
                        }
                        case NUMBER:
                            // TODO: other inputs?
                            OUTPUTF("istream = stdin; //from %c\n", reg);
                            break;
                    }
                    break;
                }
                
                // output file stream
                case 'F': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            int mode;
                            readRegister(&mode, NUMBER);
                            OUTPUTF("if(%c) {\n", mode);
                            OUTPUTF("ostream = fopen(%c->data, fileModeNumber(%c));\n", reg, mode);
                            OUTPUT("} else {\n");
                            OUTPUT("fclose(ostream);\nostream = stdout;\n");
                            OUTPUT("}\n");
                            break;
                        }
                        case NUMBER:
                            // TODO: other inputs?
                            OUTPUTF("ostream = %c == 2 ? stderr : stdout;\n", reg);
                            break;
                    }
                    break;
                }
                
                // getchar
                case 'g': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: OUTPUTF("*%c->data = fgetc(istream);\n", reg); break;
                        case NUMBER: OUTPUTF("%c = fgetc(istream);\n", reg); break;
                    }
                    break;
                }
                
                // input
                case 'i': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING:
                            // TODO: fix
                            OUTPUTF("fgets(%c->data, %c->size, stdin);\n", reg, reg);
                            break;
                        case NUMBER:
                            OUTPUTF("scanf(\" %%i\", &%c);\n", reg);
                            break;
                    }
                    break;
                }
                
                // resize
                case 'r': {
                    switch(rtype) {
                        case UNDEFINED: break;
                        case STRING: {
                            int index;
                            readRegister(&index, NUMBER);
                            OUTPUTF("s4str_resize(%c, %c);\n", reg, index);
                            break;
                        }
                        case NUMBER:
                            FAIL_UNEXPECTED(reg, rtype, STRING);
                            break;
                    }
                    break;
                }
                
                // size
                case 's': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: {
                            int arg2;
                            readRegister(&arg2, NUMBER);
                            OUTPUTF("%c = %c->size;\n", arg2, reg);
                            break;
                        }
                        case NUMBER:
                            FAIL_UNEXPECTED(reg, rtype, STRING);
                            break;
                    }
                    break;
                }
                
                // print
                case 'p': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: OUTPUTF("s4str_puts_to(%c, ostream);\n", reg); break;
                        case NUMBER: OUTPUTF("fprintf(ostream, \"%%i\\n\", %c);\n", reg); break;
                    }
                    break;
                }
                
                // if
                case '?': {
                    switch(rtype) {
                        case UNDEFINED: break; // handled by getMode
                        case STRING: FAIL_TODO(); break;
                        case NUMBER:
                            OUTPUTF("if(%c) {\n", reg);
                            expectingClose++;
                            break;
                    }
                    break;
                }
                
                default:
                    FAIL(7, "Unknown command `%c` for `%c`", cmd, reg);
                    break;
            }
        }
    }
    
    if(mode == LOOP) {
        OUTPUT("}\n");
    }
    
    for(int id = 0; id < MODE_COUNT; id++) {
        if(modes[id] == STRING) {
            int chr = idchr(id);
            OUTPUTF("s4str_free(%c);\n", chr);
        }
    }
    
    OUTPUT(boilerplate[1]);
    
    fclose(compileFile);
    
    #define COMMANDBUFSIZE (1024)
    char command[COMMANDBUFSIZE];
    snprintf(command, COMMANDBUFSIZE,
        "%s"
        COMPILE(OUTNAME, "%s")
        " && " REMOVE(OUTNAME),
        debug ? "cat " OUTNAME " && " : "",
        outputName
    );
        // " && " RUN(OUTNAME, "%s")
    
    int errco = system(command);
    if(errco) {
        fprintf(stderr, "Compilation errored with code %i. Terminating.\n", errco);
        return errco;
    }
}
