for k,v in pairs(request) do
    print(("%q: %q"):format(k, v))
end

return {
    status = 200,
    content_type = "text/plain",
    body = "Hello, Internet!\n"
}
