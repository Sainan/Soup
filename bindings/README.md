# Soup Bindings

We have code generators that produce type-safe APIs and handle freeing resources for you for the following languages:

- [Lua](https://github.com/calamity-inc/Soup/blob/senpai/bindings/soup-apigen.lua)

Otherwise, you can also just FFI without any abstractions, just be careful not to leak memory:

- [Lua](https://github.com/calamity-inc/Soup/blob/senpai/bindings/soup.lua)
- [JavaScript](../docs/bindings/js-bindings-cdn.md)
- [PHP](https://github.com/calamity-inc/Soup/blob/senpai/bindings/soup.php)
- Plus, any other language that supports FFI!

For a list of available functions and their signatures, see [soup.h](https://github.com/calamity-inc/Soup/blob/senpai/bindings/soup.h).
