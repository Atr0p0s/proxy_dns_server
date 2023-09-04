# proxy_dns_server

Dns server with blacklist support.

Configuration file: server.config.
It saves setting in JSON format, that is parsed via [cJSON](https://github.com/DaveGamble/cJSON) library.

To compile the server use: 'gcc dns_server.c -lcjson \`pkg-config --cflags --libs glib-2.0\` -o dns.out'

### Server check
dns_client.py - sends 2 requests (the first one (www.blocked.com.ua) with blocked domain).

Server output:
```
...Server configuration...
>> Upper server is set to '192.168.88.1'
>> Refused answer is set to 'Permission denied'
>> Added 2 site(s) to the blacklist

DNS proxy server is running...
Request wwwblockedcomua is blocked

Forwarding request for googlecom...
IP answer from external dns server: 142.250.203.142
```

Client output:
```
id 31758
opcode QUERY
rcode REFUSED
flags QR RD
;QUESTION
www.blocked.com.ua. IN A
;ANSWER
;AUTHORITY
;ADDITIONAL

id 4146
opcode QUERY
rcode NOERROR
flags QR RD RA
;QUESTION
google.com. IN A
;ANSWER
google.com. 245 IN A 142.250.203.142
;AUTHORITY
google.com. 80062 IN NS ns1.google.com.
google.com. 80062 IN NS ns4.google.com.
google.com. 80062 IN NS ns2.google.com.
google.com. 80062 IN NS ns3.google.com.
;ADDITIONAL
ns1.google.com. 81834 IN A 216.239.32.10
ns4.google.com. 81834 IN A 216.239.38.10
ns2.google.com. 81834 IN A 216.239.34.10
ns3.google.com. 81834 IN A 216.239.36.10
```
