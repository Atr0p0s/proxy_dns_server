import dns.query
import dns.message

# Define the custom DNS server's IP address and port
custom_dns_server = '127.0.0.1'
custom_dns_port = 53

# Create a DNS query message for the desired domain
query = dns.message.make_query('www.blocked.com.ua', dns.rdatatype.A)

# Send the query to the custom DNS server
response = dns.query.udp(query, custom_dns_server, port=custom_dns_port)

# Print the response
print(response)
print()

query = dns.message.make_query('google.com', dns.rdatatype.A)
response = dns.query.udp(query, custom_dns_server, port=custom_dns_port)
print(response)
