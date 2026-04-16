local inspect = require('inspect')

print(inspect.inspect(request))

return {
    status = 200,
    content_type = "text/plain",
    body = ("Hello, %s!\n"):format(request.queries.name or "stranger")
}
