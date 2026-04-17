local inspect = require('inspect')

print(inspect.inspect(request))

local logs = assert(io.open("logs.txt", "a"))
logs:write(("[%s] I connected with %s\n"):format(os.date("!%Y-%m-%d %H:%M:%S"), request.ip))
logs:close()

local counter = request.cookies.counter and tonumber(request.cookies.counter) or 0

return {
    status = 200,
    content_type = "text/html",
    cookies = {
        counter = counter + 1,
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

