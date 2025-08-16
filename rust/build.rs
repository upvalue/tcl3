fn main() {
    cc::Build::new()
        .file("../vendor/linenoise.c")
        .compile("linenoise");
}
