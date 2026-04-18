local inspect = require('inspect')

print(inspect.inspect(request))

local counter = request.cookies.counter and request.cookies.counter or "|"

return {
    status = 200,
    content_type = "text/html",
    cookies = {
        counter = {value = ("| %s"):format(counter), same_site = 'none'},
    },
    body = ([[
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8" />
            <title>Foobar</title>
        </head>
        <body>
            <h1>Hello, %s!</h1>
        </body>
        </html>
    ]]):format(request.queries.name or "stranger")
}

