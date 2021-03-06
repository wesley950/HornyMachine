#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dlfcn.h>

typedef struct OBJECT_t
{
    u_int8_t type;

    union
    {
        u_int8_t u8;
        int8_t i8;
        u_int32_t u32;
        int32_t i32;
        void* pData;
    };
} OBJECT;

typedef struct STACK_t
{
    int top;
    int size;
    OBJECT* stack;
} STACK;

typedef uint8_t* (*instruction)(uint8_t*, STACK*);

STACK create_stack(int size)
{
    STACK s;
    s.top = 0;
    s.size = size;
    s.stack = (OBJECT*) malloc(sizeof(OBJECT) * size);
    return s;
}

int stack_push(STACK* s, OBJECT o)
{
    s->stack[s->top++] = o;
    return s->top;
}

OBJECT stack_pop(STACK* s)
{
    return s->stack[--(s->top)];
}

OBJECT stack_peek(STACK* s)
{
    return s->stack[s->top - 1];
}

void usage()
{
    printf("Usage: horny <file>\n");
    exit(1);
}

uint8_t* load_file(char* filename)
{
    FILE* file;
    int size;
    uint8_t* code = NULL;
    struct stat st;

    if ((file = fopen(filename, "r")))
    {
        fstat(fileno(file), &st);
        code = (uint8_t*) malloc(sizeof(uint8_t) * st.st_size);
        fread((void*) code, 1, st.st_size, file);
    }
    else
    {
        printf("Error: cannot open file %s\n", filename);
        usage();
    }

    return code;
}

uint8_t* put_c(uint8_t* ip, STACK* s)
{
    int c = *(ip + 1);

    putchar(c);

    return ip + 2;
}

uint8_t* emit(uint8_t* ip, STACK* s)
{
    OBJECT o;

    o = stack_pop(s);

    switch (o.type)
    {
    case 'c':
        putchar(o.i8);
        break;

    case 's':
        printf("%s", o.pData);
        break;
    }

    return ip + 1;
}

uint8_t* push_c(uint8_t* ip, STACK* s)
{
    OBJECT c;
    c.type = 'c';
    c.i8 = *(ip + 1);

    stack_push(s, c);

    return ip + 2;
}

uint8_t* push_sz(uint8_t* ip, STACK* s)
{
    char* str = (char*)(ip + 1);
    uint8_t len = std::strlen(str);
    char* pData = (char*) std::malloc(1 + len);
    OBJECT strObj;

    std::strcpy(pData, str);
    pData[len] = '\0';

    strObj.type = 's';
    strObj.pData = pData;

    stack_push(s, strObj);

    return ip + len;
}

uint8_t* add_c(uint8_t* ip, STACK* s)
{
    OBJECT c1;
    OBJECT c2;
    OBJECT r;

    c1 = stack_pop(s);
    c2 = stack_pop(s);
    r.type = 'c';
    r.i8 = c1.i8 + c2.i8;

    stack_push(s, r);

    return ip + 1;
}

uint8_t* load_lib(uint8_t* ip, STACK* s)
{
    OBJECT o = stack_pop(s);
    OBJECT o2 = stack_pop(s);

    void* handle;
    void (*hello)(void);
    char* error;

    handle = dlopen((char*)o.pData, RTLD_LAZY);

    if (!handle)
    {
        printf("The library %s could not be loaded or found!\n", (char*)o.pData);
        exit('l');
    }

    dlerror();

    hello = (void(*)(void)) dlsym(handle, (char*) o2.pData);

    error = dlerror();
    if (error)
    {
        printf("%s\n", error);
        exit('m');
    }

    hello();
    dlclose(handle);

    return ip + 1;
}

uint8_t* no_op(uint8_t* ip, STACK* s)
{
    return ip + 1;
}

int main(int argc, char** argv)
{
    uint8_t* code;
    uint8_t* ip;
    STACK data;
    instruction ops[256];

    if (argc != 2)
    {
        usage();
    }

    for (int i = 0;i < 256;i++)
    {
        ops[i] = no_op;
    }

    ops['p'] = put_c;
    ops['c'] = push_c;
    ops['a'] = add_c;
    ops['e'] = emit;
    ops['s'] = push_sz;
    ops['l'] = load_lib;

    code = load_file(argv[1]);
    data = create_stack(1024);
    ip = code;

    while (*ip != 'h')
    {
        ip = ops[*ip](ip, &data);
    }

    return 0;
}
