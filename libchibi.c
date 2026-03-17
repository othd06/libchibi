
#include "chibicc.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>


static FILE *open_file(char *path) {
    if (!path || strcmp(path, "-") == 0)
        return stdout;

    FILE *out = fopen(path, "w");
    if (!out)
        error("cannot open output file: %s: %s", path, strerror(errno));
    return out;
}

static void cProg(Obj *prog, char *output_file) {
    // Open a temporary output buffer.
    char *buf;
    size_t buflen;
    FILE *output_buf = open_memstream(&buf, &buflen);

    // Traverse the AST to emit assembly.
    codegen(prog, output_buf);
    fclose(output_buf);

    // Write the asembly text to a file.
    FILE *out = open_file(output_file);
    fwrite(buf, buflen, 1, out);
    fclose(out);
}

static void aProg(Obj *prog, char *output_file) {
    // 1. Create a temporary file for the assembly output.
    char asm_template[] = "/tmp/libchibi-XXXXXX.s";

    int fd = mkstemp(asm_template);
    if (fd < 0)
        error("mkstemp failed: %s", strerror(errno));

    FILE *tmp = fdopen(fd, "w");
    if (!tmp)
        error("fdopen failed: %s", strerror(errno));

    // 2. Generate assembly into the temporary file.
    fclose(tmp); 
    cProg(prog, asm_template);

    // 3. Fork and exec the assembler.
    pid_t pid = fork();
    if (pid < 0)
        error("fork failed: %s", strerror(errno));

    if (pid == 0) {
        // Child: exec 'as'
        execlp("as", "as", "-o", output_file, asm_template, (char *)NULL);
        // If exec fails:
        fprintf(stderr, "as: %s\n", strerror(errno));
        _exit(1);
    }

    // 4. Parent: wait for assembler to finish.
    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        error("assembler failed");

    // 5. Clean up the temporary assembly file.
    unlink(asm_template);
}


