local inspect = require('inspect')

print(inspect.inspect(request))

return {
    status = 200,
    content_type = "text/html",
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
